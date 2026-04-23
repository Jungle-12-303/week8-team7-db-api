param(
    [Parameter(Mandatory = $true)]
    [string]$Topic
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$codexDir = Split-Path -Parent $scriptDir
$sessionsDir = Join-Path $codexDir "sessions"

if (-not (Test-Path -LiteralPath $sessionsDir)) {
    New-Item -ItemType Directory -Path $sessionsDir | Out-Null
}

$safeTopic = ($Topic -replace '[^a-zA-Z0-9_-]', '-').Trim('-')
if ([string]::IsNullOrWhiteSpace($safeTopic)) {
    $safeTopic = "note"
}

$timestamp = Get-Date -Format "yyyy-MM-dd-HHmm"
$filename = "$timestamp-$safeTopic.md"
$path = Join-Path $sessionsDir $filename

$content = @(
    "# Session Note"
    ""
    "- timestamp: " + (Get-Date).ToString("yyyy-MM-ddTHH:mm:sszzz")
    "- topic: " + $Topic
    ""
    "## Summary"
    "- "
    ""
    "## Details"
    "- "
    ""
    "## Next"
    "- "
) -join [Environment]::NewLine

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($path, $content + [Environment]::NewLine, $utf8NoBom)

Write-Output $path
