param(
    [Parameter(Mandatory = $true)]
    [string]$Summary,

    [string]$Scope = "root",

    [string]$Actor = "codex",

    [string]$Source = "manual"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$codexDir = Split-Path -Parent $scriptDir
$historyPath = Join-Path $codexDir "history.jsonl"

if (-not (Test-Path -LiteralPath $codexDir)) {
    New-Item -ItemType Directory -Path $codexDir | Out-Null
}

$record = [ordered]@{
    timestamp = (Get-Date).ToString("yyyy-MM-ddTHH:mm:sszzz")
    scope = $Scope
    actor = $Actor
    summary = $Summary
    source = $Source
}

$line = $record | ConvertTo-Json -Compress
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::AppendAllText($historyPath, $line + [Environment]::NewLine, $utf8NoBom)

Write-Output $line
