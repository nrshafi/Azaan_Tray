param (
    [int]$ProcessId,
    [string]$ProcessName = "WaqtTray"
)

if (-not $ProcessId) {
    $proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $proc) {
        Write-Error "Process $ProcessName not found."
        exit 1
    }
    $ProcessId = $proc.Id
}

$proc = Get-Process -Id $ProcessId -ErrorAction Stop

# Working set in MB
$wsMB = [math]::Round($proc.WorkingSet64 / 1MB, 2)
# Private memory (commit) in MB
$privMB = [math]::Round($proc.PrivateMemorySize64 / 1MB, 2)
# Handles
$handles = $proc.HandleCount
# Threads
$threads = $proc.Threads.Count

# Query GDI and USER objects via Win32 API
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Win32Metrics {
    [DllImport("user32.dll")]
    public static extern uint GetGuiResources(IntPtr hProcess, uint uiFlags);
}
"@ -ErrorAction SilentlyContinue

$gdiObjects = [Win32Metrics]::GetGuiResources($proc.Handle, 0)
$userObjects = [Win32Metrics]::GetGuiResources($proc.Handle, 1)

[PSCustomObject]@{
    ProcessName     = $proc.ProcessName
    PID             = $proc.Id
    WorkingSetMB    = $wsMB
    PrivateBytesMB  = $privMB
    Handles         = $handles
    Threads         = $threads
    GDIObjects      = $gdiObjects
    UserObjects     = $userObjects
} | Format-List
