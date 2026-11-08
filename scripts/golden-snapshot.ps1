# scripts/golden-snapshot.ps1
# Runs native/bin/azaan.exe --json across a fixed matrix of 12 cities x 12 dates
# Generates or verifies the golden snapshot and outputs its SHA-256.

param (
    [string]$OutputFile = "",
    [switch]$UpdateBaseline = $false
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path "$PSScriptRoot\.."
$exePath = Join-Path $repoRoot "native\bin\azaan.exe"
$goldenDir = Join-Path $repoRoot "native\tests\golden"
$baselineFile = Join-Path $goldenDir "baseline.json"
$snapshotFile = Join-Path $goldenDir "snapshot.json"

if (-not (Test-Path $exePath)) {
    Write-Host "[golden-snapshot] azaan.exe not found, building..."
    & make -C (Join-Path $repoRoot "native") cli
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to build native/bin/azaan.exe"
    }
}

if (-not (Test-Path $goldenDir)) {
    New-Item -ItemType Directory -Path $goldenDir -Force | Out-Null
}

$cities = @(
    @{ Name = "Dhaka";      Lat =  23.8103; Lng =   90.4125 },
    @{ Name = "Mecca";      Lat =  21.4225; Lng =   39.8262 },
    @{ Name = "London";     Lat =  51.5074; Lng =   -0.1278 },
    @{ Name = "Oslo";       Lat =  59.9139; Lng =   10.7522 },
    @{ Name = "Tromso";     Lat =  69.6492; Lng =   18.9553 },
    @{ Name = "Reykjavik";  Lat =  64.1466; Lng =  -21.9426 },
    @{ Name = "Sydney";     Lat = -33.8688; Lng =  151.2093 },
    @{ Name = "Cape Town";  Lat = -33.9249; Lng =   18.4241 },
    @{ Name = "New York";   Lat =  40.7128; Lng =  -74.0060 },
    @{ Name = "Anchorage";  Lat =  61.2181; Lng = -149.9003 },
    @{ Name = "Singapore";  Lat =   1.3521; Lng =  103.8198 },
    @{ Name = "Quito";      Lat =  -0.1807; Lng =  -78.4678 }
)

$dates = @(
    "2024-06-20", # Summer solstice 2024
    "2024-12-21", # Winter solstice 2024
    "2024-03-20", # Spring equinox 2024
    "2024-09-22", # Autumn equinox 2024
    "2024-02-29", # Leap day 2024
    "2023-12-31", # Year-end boundary
    "2024-01-01", # Year-start boundary
    "2024-03-12", # Ramadan date
    "2024-07-15", # High-latitude summer
    "2024-01-15", # High-latitude winter
    "2023-06-21", # Summer solstice 2023
    "2023-03-21"  # Equinox 2023
)

$targetFile = if ($OutputFile -ne "") {
    $OutputFile
} elseif ($UpdateBaseline -or (-not (Test-Path $baselineFile))) {
    $baselineFile
} else {
    $snapshotFile
}

$sb = [System.Text.StringBuilder]::new()
$culture = [System.Globalization.CultureInfo]::InvariantCulture

foreach ($c in $cities) {
    $latStr = $c.Lat.ToString("F4", $culture)
    $lngStr = $c.Lng.ToString("F4", $culture)
    foreach ($d in $dates) {
        $pinfo = [System.Diagnostics.ProcessStartInfo]::new()
        $pinfo.FileName = $exePath
        $pinfo.Arguments = "--json --date $d --lat $latStr --lng $lngStr"
        $pinfo.RedirectStandardOutput = $true
        $pinfo.RedirectStandardError = $true
        $pinfo.UseShellExecute = $false
        $pinfo.CreateNoWindow = $true

        $proc = [System.Diagnostics.Process]::Start($pinfo)
        $out = $proc.StandardOutput.ReadToEnd()
        $err = $proc.StandardError.ReadToEnd()
        $proc.WaitForExit()

        if ($proc.ExitCode -ne 0) {
            throw "azaan.exe failed for city $($c.Name) ($latStr, $lngStr) on date $d : $err"
        }

        # Normalize line endings to LF for byte-level determinism
        $normalized = $out.Replace("`r`n", "`n").TrimEnd()
        [void]$sb.Append($normalized)
        [void]$sb.Append("`n---`n")
    }
}

$contentBytes = [System.Text.Encoding]::UTF8.GetBytes($sb.ToString())
[System.IO.File]::WriteAllBytes($targetFile, $contentBytes)

$sha256 = [System.Security.Cryptography.SHA256]::Create()
$hashBytes = $sha256.ComputeHash($contentBytes)
$hashStr = -join ($hashBytes | ForEach-Object { "{0:x2}" -f $_ })

Write-Host "Snapshot target: $targetFile"
Write-Host "Golden SHA-256:  $hashStr"

if ($targetFile -ne $baselineFile -and (Test-Path $baselineFile)) {
    $baselineBytes = [System.IO.File]::ReadAllBytes($baselineFile)
    $baselineHashBytes = $sha256.ComputeHash($baselineBytes)
    $baselineHashStr = -join ($baselineHashBytes | ForEach-Object { "{0:x2}" -f $_ })

    if ($hashStr -eq $baselineHashStr) {
        Write-Host "Golden check:    MATCH (identical to baseline)"
        # Remove transient snapshot file if identical
        Remove-Item -Path $targetFile -Force -ErrorAction SilentlyContinue
    } else {
        Write-Host "Golden check:    MISMATCH (baseline is $baselineHashStr)"
        exit 1
    }
} else {
    Write-Host "Golden baseline established at $targetFile"
}
