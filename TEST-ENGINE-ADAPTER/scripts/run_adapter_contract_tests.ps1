Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir
$buildDir = Join-Path $projectDir 'build'

$binaryPath = 'build/adapter_contract_test.exe'
$includeDirs = @(
    '../SERVER-CORE/include'
    '../week8-team7-db-api/include'
)
$sources = @(
    'scripts/adapter_contract_test.c'
    '../SERVER-CORE/src/engine_api.c'
    '../week8-team7-db-api/src/common/util.c'
    '../week8-team7-db-api/src/storage/schema.c'
    '../week8-team7-db-api/src/storage/storage.c'
    '../week8-team7-db-api/src/sql/ast.c'
    '../week8-team7-db-api/src/sql/lexer.c'
    '../week8-team7-db-api/src/sql/parser.c'
    '../week8-team7-db-api/src/execution/executor.c'
    '../week8-team7-db-api/src/index/bptree.c'
    '../week8-team7-db-api/src/index/table_index.c'
)

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

Push-Location $projectDir
try {
    $compileArgs = @('-Wall', '-Wextra', '-std=c11')
    foreach ($includeDir in $includeDirs) {
        $compileArgs += "-I$includeDir"
    }
    $compileArgs += @('-o', $binaryPath)
    $compileArgs += $sources

    & gcc @compileArgs
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    & $binaryPath
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
