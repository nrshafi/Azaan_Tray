# Session environment script for Waqt-Tray development
$toolsBin = Join-Path $PSScriptRoot "..\tools\w64devkit\bin"
if (Test-Path $toolsBin) {
    $env:PATH = "$toolsBin;" + $env:PATH
    Write-Host "[Waqt-Tray] Added w64devkit to session PATH: $toolsBin" -ForegroundColor Green
    & "$toolsBin\gcc.exe" --version | Select-Object -First 1
    & "$toolsBin\make.exe" --version | Select-Object -First 1
} else {
    Write-Warning "[Waqt-Tray] tools\w64devkit\bin not found!"
}
