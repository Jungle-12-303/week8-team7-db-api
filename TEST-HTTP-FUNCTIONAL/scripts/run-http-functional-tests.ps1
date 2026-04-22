param(
    [string]$BaseUrl = "http://127.0.0.1:8080",
    [string]$CasesDir = (Join-Path $PSScriptRoot "..\cases"),
    [string[]]$CaseName,
    [ValidateSet("direct", "docker-compose-exec")]
    [string]$Transport = "direct",
    [string]$DockerComposeFile = (Join-Path $PSScriptRoot "..\..\compose.yaml"),
    [string]$DockerService = "server",
    [switch]$ListCases,
    [switch]$ValidateOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

function ConvertTo-Hashtable {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Value
    )

    if ($null -eq $Value) {
        return $null
    }

    if ($Value -is [System.Collections.IDictionary]) {
        $table = @{}
        foreach ($key in $Value.Keys) {
            $table[$key] = ConvertTo-Hashtable -Value $Value[$key]
        }
        return $table
    }

    if ($Value -is [System.Collections.IEnumerable] -and -not ($Value -is [string])) {
        $items = @()
        foreach ($item in $Value) {
            $items += ,(ConvertTo-Hashtable -Value $item)
        }
        return $items
    }

    if ($Value -is [pscustomobject]) {
        $table = @{}
        foreach ($property in $Value.PSObject.Properties) {
            $table[$property.Name] = ConvertTo-Hashtable -Value $property.Value
        }
        return $table
    }

    return $Value
}

function Get-CaseFiles {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "cases directory not found: $Path"
    }

    return Get-ChildItem -LiteralPath $Path -Filter *.json | Sort-Object Name
}

function Read-CaseDefinition {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $raw = Get-Content -Raw -LiteralPath $Path -Encoding UTF8
    $parsed = $raw | ConvertFrom-Json
    $case = ConvertTo-Hashtable -Value $parsed
    $case["__path"] = $Path
    return $case
}

function Get-ObjectPathValue {
    param(
        [Parameter(Mandatory = $true)]
        [AllowNull()]
        [object]$InputObject,
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $current = $InputObject
    foreach ($segment in $Path.Split(".")) {
        if ($null -eq $current) {
            throw "path '$Path' is missing at '$segment'"
        }

        if ($current -is [System.Collections.IDictionary]) {
            if (-not $current.Contains($segment)) {
                throw "path '$Path' is missing at '$segment'"
            }
            $current = $current[$segment]
            continue
        }

        if ($current -is [pscustomobject]) {
            $property = $current.PSObject.Properties[$segment]
            if ($null -eq $property) {
                throw "path '$Path' is missing at '$segment'"
            }
            $current = $property.Value
            continue
        }

        throw "path '$Path' cannot descend into value of type '$($current.GetType().FullName)'"
    }

    return $current
}

function Get-ValueTypeName {
    param(
        [AllowNull()]
        [object]$Value
    )

    if ($null -eq $Value) {
        return "null"
    }

    if ($Value -is [bool]) {
        return "boolean"
    }

    if ($Value -is [Byte] -or
        $Value -is [SByte] -or
        $Value -is [Int16] -or
        $Value -is [UInt16] -or
        $Value -is [Int32] -or
        $Value -is [UInt32] -or
        $Value -is [Int64] -or
        $Value -is [UInt64]) {
        return "integer"
    }

    if ($Value -is [Single] -or
        $Value -is [Double] -or
        $Value -is [Decimal]) {
        return "number"
    }

    if ($Value -is [string]) {
        return "string"
    }

    if ($Value -is [System.Collections.IDictionary] -or $Value -is [pscustomobject]) {
        return "object"
    }

    if ($Value -is [System.Collections.IEnumerable]) {
        return "array"
    }

    return $Value.GetType().Name
}

function Assert-Condition {
    param(
        [Parameter(Mandatory = $true)]
        [bool]$Condition,
        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Invoke-ExternalCapture {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        [Parameter(Mandatory = $true)]
        [string[]]$ArgumentList
    )

    $output = & $FilePath @ArgumentList 2>&1
    $exitCode = $LASTEXITCODE
    $text = ($output | ForEach-Object { [string]$_ }) -join [Environment]::NewLine

    return @{
        exitCode = $exitCode
        text = $text
    }
}

function Normalize-ResponseJson {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Body
    )

    if ([string]::IsNullOrWhiteSpace($Body)) {
        throw "response body is empty"
    }

    return ConvertTo-Hashtable -Value ($Body | ConvertFrom-Json)
}

function Write-RequestBody {
    param(
        [Parameter(Mandatory = $true)]
        [System.Net.HttpWebRequest]$Request,
        [AllowNull()]
        [object]$RequestDefinition
    )

    if ($null -eq $RequestDefinition) {
        return
    }

    $bodyText = $null
    if ($RequestDefinition.Contains("rawBody")) {
        $bodyText = [string]$RequestDefinition["rawBody"]
    } elseif ($RequestDefinition.Contains("jsonBody")) {
        $bodyText = ($RequestDefinition["jsonBody"] | ConvertTo-Json -Depth 100 -Compress)
    }

    if ($null -eq $bodyText) {
        return
    }

    $bodyBytes = [System.Text.Encoding]::UTF8.GetBytes($bodyText)
    $Request.ContentLength = $bodyBytes.Length
    $stream = $Request.GetRequestStream()
    try {
        $stream.Write($bodyBytes, 0, $bodyBytes.Length)
    } finally {
        $stream.Dispose()
    }
}

function Invoke-CaseRequest {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BaseUrlValue,
        [Parameter(Mandatory = $true)]
        [hashtable]$Case
    )

    if ($script:Transport -eq "docker-compose-exec") {
        return Invoke-CaseRequestDockerCompose -BaseUrlValue $BaseUrlValue -Case $Case
    }

    return Invoke-CaseRequestDirect -BaseUrlValue $BaseUrlValue -Case $Case
}

function Invoke-CaseRequestDirect {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BaseUrlValue,
        [Parameter(Mandatory = $true)]
        [hashtable]$Case
    )

    $requestDefinition = $Case["request"]
    $uri = [System.Uri]::new(($BaseUrlValue.TrimEnd("/")) + [string]$requestDefinition["path"])
    $request = [System.Net.HttpWebRequest]::Create($uri)
    $request.Method = [string]$requestDefinition["method"]
    $request.Accept = "*/*"
    $request.Timeout = 15000
    $request.ReadWriteTimeout = 15000
    $request.AutomaticDecompression = [System.Net.DecompressionMethods]::GZip -bor [System.Net.DecompressionMethods]::Deflate

    if ($requestDefinition.Contains("headers")) {
        foreach ($entry in $requestDefinition["headers"].GetEnumerator()) {
            $name = [string]$entry.Key
            $value = [string]$entry.Value
            switch ($name.ToLowerInvariant()) {
                "content-type" { $request.ContentType = $value }
                "accept" { $request.Accept = $value }
                "user-agent" { $request.UserAgent = $value }
                default { $request.Headers[$name] = $value }
            }
        }
    }

    if ($request.Method -in @("POST", "PUT", "PATCH")) {
        Write-RequestBody -Request $request -RequestDefinition $requestDefinition
    }

    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

    try {
        try {
            $response = $request.GetResponse()
        } catch [System.Net.WebException] {
            if ($null -eq $_.Exception.Response) {
                throw "request failed before receiving an HTTP response: $($_.Exception.Message)"
            }
            $response = $_.Exception.Response
        }

        try {
            $statusCode = [int]([System.Net.HttpWebResponse]$response).StatusCode
            $contentType = $response.ContentType
            $reader = [System.IO.StreamReader]::new($response.GetResponseStream(), [System.Text.Encoding]::UTF8)
            try {
                $body = $reader.ReadToEnd()
            } finally {
                $reader.Dispose()
            }

            return @{
                statusCode = $statusCode
                contentType = $contentType
                body = $body
                durationMs = [int][math]::Round($stopwatch.Elapsed.TotalMilliseconds)
            }
        } finally {
            $response.Dispose()
        }
    } finally {
        $stopwatch.Stop()
    }
}

function Invoke-CaseRequestDockerCompose {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BaseUrlValue,
        [Parameter(Mandatory = $true)]
        [hashtable]$Case
    )

    $requestDefinition = $Case["request"]
    $requestUrl = ($BaseUrlValue.TrimEnd("/")) + [string]$requestDefinition["path"]
    $curlArguments = @(
        "compose",
        "-f",
        $script:DockerComposeFile,
        "exec",
        "-T",
        $script:DockerService,
        "curl",
        "--silent",
        "--show-error",
        "--request",
        [string]$requestDefinition["method"]
    )

    if ($requestDefinition.Contains("headers")) {
        foreach ($entry in $requestDefinition["headers"].GetEnumerator()) {
            $curlArguments += @(
                "--header",
                ("{0}: {1}" -f [string]$entry.Key, [string]$entry.Value)
            )
        }
    }

    if ($requestDefinition.Contains("rawBody")) {
        $curlArguments += @(
            "--data-binary",
            [string]$requestDefinition["rawBody"]
        )
    } elseif ($requestDefinition.Contains("jsonBody")) {
        $curlArguments += @(
            "--data-binary",
            ($requestDefinition["jsonBody"] | ConvertTo-Json -Depth 100 -Compress)
        )
    }

    $metadataMarker = "__CODEX_HTTP_META__"
    $writeOut = $metadataMarker + "http_code=%{http_code}`ncontent_type=%{content_type}`ntime_total=%{time_total}"
    $curlArguments += @(
        "--write-out",
        $writeOut,
        $requestUrl
    )

    $dockerResult = Invoke-ExternalCapture -FilePath "docker" -ArgumentList $curlArguments

    if ([int]$dockerResult["exitCode"] -ne 0) {
        throw "docker-compose-exec transport failed: $($dockerResult["text"])"
    }

    $rawText = [string]$dockerResult["text"]
    $markerIndex = $rawText.LastIndexOf($metadataMarker)
    if ($markerIndex -lt 0) {
        throw "docker-compose-exec response metadata is missing"
    }

    $body = $rawText.Substring(0, $markerIndex)
    $metadataText = $rawText.Substring($markerIndex + $metadataMarker.Length)
    $metadata = @{}
    foreach ($line in ($metadataText -split "\r?\n")) {
        if ([string]::IsNullOrWhiteSpace($line)) {
            continue
        }

        $parts = $line -split "=", 2
        if ($parts.Count -ne 2) {
            throw "docker-compose-exec metadata line is malformed: $line"
        }

        $metadata[$parts[0]] = $parts[1]
    }

    return @{
        statusCode = [int]$metadata["http_code"]
        contentType = [string]$metadata["content_type"]
        body = $body
        durationMs = [int][math]::Round(([double]$metadata["time_total"]) * 1000.0)
    }
}

function Validate-CaseDefinition {
    param(
        [Parameter(Mandatory = $true)]
        [hashtable]$Case
    )

    foreach ($field in @("name", "request", "expect")) {
        Assert-Condition -Condition $Case.Contains($field) -Message "case '$($Case["__path"])' is missing '$field'"
    }

    $requestDefinition = $Case["request"]
    foreach ($field in @("method", "path")) {
        Assert-Condition -Condition $requestDefinition.Contains($field) -Message "case '$($Case["name"])' is missing request.$field"
    }

    $expect = $Case["expect"]
    Assert-Condition -Condition $expect.Contains("statusCode") -Message "case '$($Case["name"])' is missing expect.statusCode"
}

function Test-ResponseAgainstExpectation {
    param(
        [Parameter(Mandatory = $true)]
        [hashtable]$Case,
        [Parameter(Mandatory = $true)]
        [hashtable]$Response
    )

    $expect = $Case["expect"]
    Assert-Condition -Condition ($Response["statusCode"] -eq [int]$expect["statusCode"]) -Message "expected HTTP $($expect["statusCode"]), got $($Response["statusCode"])"

    if ($expect.Contains("contentTypeContains")) {
        Assert-Condition -Condition ([string]$Response["contentType"]).ToLowerInvariant().Contains(([string]$expect["contentTypeContains"]).ToLowerInvariant()) -Message "expected Content-Type containing '$($expect["contentTypeContains"])', got '$($Response["contentType"])'"
    }

    $json = Normalize-ResponseJson -Body ([string]$Response["body"])

    if ($expect.Contains("requiredFields")) {
        foreach ($path in $expect["requiredFields"]) {
            [void](Get-ObjectPathValue -InputObject $json -Path ([string]$path))
        }
    }

    if ($expect.Contains("fieldTypes")) {
        foreach ($entry in $expect["fieldTypes"].GetEnumerator()) {
            $actualValue = Get-ObjectPathValue -InputObject $json -Path ([string]$entry.Key)
            $actualType = Get-ValueTypeName -Value $actualValue
            $expectedType = [string]$entry.Value

            if ($expectedType -eq "number") {
                Assert-Condition -Condition ($actualType -in @("integer", "number")) -Message "field '$($entry.Key)' expected type '$expectedType', got '$actualType'"
            } else {
                Assert-Condition -Condition ($actualType -eq $expectedType) -Message "field '$($entry.Key)' expected type '$expectedType', got '$actualType'"
            }
        }
    }

    if ($expect.Contains("fieldEquals")) {
        foreach ($entry in $expect["fieldEquals"].GetEnumerator()) {
            $actualValue = Get-ObjectPathValue -InputObject $json -Path ([string]$entry.Key)
            $expectedValue = $entry.Value
            Assert-Condition -Condition ($actualValue -eq $expectedValue) -Message "field '$($entry.Key)' expected '$expectedValue', got '$actualValue'"
        }
    }

    if ($expect.Contains("fieldContains")) {
        foreach ($entry in $expect["fieldContains"].GetEnumerator()) {
            $actualValue = [string](Get-ObjectPathValue -InputObject $json -Path ([string]$entry.Key))
            $expectedFragment = [string]$entry.Value
            Assert-Condition -Condition $actualValue.Contains($expectedFragment) -Message "field '$($entry.Key)' does not contain '$expectedFragment'"
        }
    }

    if ($expect.Contains("fieldNonEmpty")) {
        foreach ($path in $expect["fieldNonEmpty"]) {
            $actualValue = Get-ObjectPathValue -InputObject $json -Path ([string]$path)
            if ($actualValue -is [string]) {
                Assert-Condition -Condition (-not [string]::IsNullOrWhiteSpace($actualValue)) -Message "field '$path' must be non-empty"
            } elseif ($actualValue -is [System.Collections.IEnumerable]) {
                $count = @($actualValue).Count
                Assert-Condition -Condition ($count -gt 0) -Message "field '$path' must be non-empty"
            } else {
                Assert-Condition -Condition ($null -ne $actualValue) -Message "field '$path' must not be null"
            }
        }
    }

    if ($expect.Contains("minDurationMs")) {
        Assert-Condition -Condition ($Response.Contains("durationMs")) -Message "response does not include durationMs"
        Assert-Condition -Condition ([int]$Response["durationMs"] -ge [int]$expect["minDurationMs"]) -Message "expected duration >= $($expect["minDurationMs"])ms, got $($Response["durationMs"])ms"
    }

    if ($expect.Contains("maxDurationMs")) {
        Assert-Condition -Condition ($Response.Contains("durationMs")) -Message "response does not include durationMs"
        Assert-Condition -Condition ([int]$Response["durationMs"] -le [int]$expect["maxDurationMs"]) -Message "expected duration <= $($expect["maxDurationMs"])ms, got $($Response["durationMs"])ms"
    }
}

$caseFiles = Get-CaseFiles -Path (Resolve-Path -LiteralPath $CasesDir)
$cases = @()

foreach ($file in $caseFiles) {
    $case = Read-CaseDefinition -Path $file.FullName
    Validate-CaseDefinition -Case $case
    if ($CaseName -and ($CaseName -notcontains [string]$case["name"])) {
        continue
    }
    $cases += ,$case
}

if ($cases.Count -eq 0) {
    throw "no matching case files were found"
}

if ($ListCases) {
    foreach ($case in $cases) {
        Write-Output ("{0}`t{1}" -f $case["name"], $case["__path"])
    }
    exit 0
}

if ($ValidateOnly) {
    Write-Output ("validated {0} case file(s)" -f $cases.Count)
    exit 0
}

$failed = @()

foreach ($case in $cases) {
    $caseNameValue = [string]$case["name"]
    try {
        $response = Invoke-CaseRequest -BaseUrlValue $BaseUrl -Case $case
        Test-ResponseAgainstExpectation -Case $case -Response $response
        Write-Output ("PASS {0}" -f $caseNameValue)
    } catch {
        $message = $_.Exception.Message
        $failed += @{
            name = $caseNameValue
            message = $message
        }
        Write-Output ("FAIL {0} - {1}" -f $caseNameValue, $message)
    }
}

if ($failed.Count -gt 0) {
    Write-Output ""
    Write-Output ("summary: {0}/{1} failed" -f $failed.Count, $cases.Count)
    exit 1
}

Write-Output ""
Write-Output ("summary: {0}/{0} passed" -f $cases.Count)
