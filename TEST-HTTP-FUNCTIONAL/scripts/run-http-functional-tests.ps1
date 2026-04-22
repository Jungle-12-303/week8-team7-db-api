param(
    [string]$BaseUrl = "http://127.0.0.1:8080",
    [string]$CasesDir = (Join-Path $PSScriptRoot "..\cases"),
    [string[]]$CaseName,
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

    if ($Value -is [byte] -or
        $Value -is [sbyte] -or
        $Value -is [short] -or
        $Value -is [ushort] -or
        $Value -is [int] -or
        $Value -is [uint] -or
        $Value -is [long] -or
        $Value -is [ulong]) {
        return "integer"
    }

    if ($Value -is [float] -or
        $Value -is [double] -or
        $Value -is [decimal]) {
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
        }
    } finally {
        $response.Dispose()
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
