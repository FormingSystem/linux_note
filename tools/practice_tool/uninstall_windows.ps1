param(
    [switch]$Clean,
    [switch]$Yes
)

$ErrorActionPreference = "Stop"
$practiceDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$registryPath = Join-Path $practiceDir ".local\software_registry.tsv"

if (-not $Clean) {
    throw "This entry only removes a tool-owned MSYS2 root. Run: .\uninstall_windows.ps1 -Clean"
}
if (-not (Test-Path -LiteralPath $registryPath)) {
    throw "Software registry not found; refusing to remove any MSYS2 installation."
}

$records = Import-Csv -Delimiter "`t" -LiteralPath $registryPath
$rootRecord = $records |
    Where-Object { $_.software_id -eq "msys2-root" } |
    Select-Object -Last 1
if (-not $rootRecord -or $rootRecord.ownership -ne "tool-owned" -or $rootRecord.cleanup_kind -ne "msys-root") {
    Write-Host "[practice] MSYS2 is external or was externally updated; clean uninstall will preserve it."
    exit 0
}

$installRoot = [IO.Path]::GetFullPath($rootRecord.managed_path)
if ($installRoot -notmatch "^[A-Za-z]:\\[^\\]+$") {
    throw "Refusing an unexpected MSYS2 root path: $installRoot"
}
if (-not $Yes) {
    $answer = Read-Host "[practice] Remove tool-owned MSYS2 at $installRoot ? [y/N]"
    if ($answer -notin @("y", "Y", "yes", "YES")) {
        Write-Host "[practice] Uninstall cancelled."
        exit 0
    }
}

$uninstaller = Join-Path $installRoot "uninstall.exe"
if (-not (Test-Path -LiteralPath $uninstaller)) {
    throw "Official MSYS2 uninstaller not found: $uninstaller"
}
$process = Start-Process -FilePath $uninstaller -ArgumentList @(
    "in",
    "--confirm-command",
    "--accept-messages"
) -Wait -PassThru
if ($process.ExitCode -ne 0) {
    throw "MSYS2 uninstaller failed with exit code $($process.ExitCode)."
}
Remove-Item -LiteralPath $registryPath -Force -ErrorAction SilentlyContinue
Write-Host "[practice] Tool-owned MSYS2 was removed."
