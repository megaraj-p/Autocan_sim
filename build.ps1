$ErrorActionPreference = 'Stop'
$sourceFiles = (Get-ChildItem .\src\*.cpp).FullName
& g++ -std=c++11 -Wall -Wextra -Wpedantic -Iinclude $sourceFiles -pthread -o autocan.exe
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
Write-Host 'Build successful: autocan.exe' -ForegroundColor Green
