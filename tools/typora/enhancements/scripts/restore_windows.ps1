[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$backup_root
)

$ErrorActionPreference = "Stop"
$typora_tools_root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$environment_helper = Join-Path $typora_tools_root "scripts\lib\typora_environment.ps1"
if (-not (Test-Path -LiteralPath $environment_helper -PathType Leaf)) {
    throw "Typora environment helper is missing: $environment_helper"
}
. $environment_helper
$backup_root = convert_typora_input_path $backup_root
$backup_root = (Resolve-Path -LiteralPath $backup_root).Path
$manifest_path = Join-Path $backup_root "manifest.json"
$window_backup = Join-Path $backup_root "window.html"

foreach ($required in @($manifest_path, $window_backup)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required backup file is missing: $required"
    }
}

$manifest = Get-Content -LiteralPath $manifest_path -Raw | ConvertFrom-Json
if (-not (Test-Path -LiteralPath $manifest.window_html -PathType Leaf)) {
    throw "Current Typora window.html is missing: $($manifest.window_html)"
}
$resolved_typora_root = get_typora_root_from_candidate $manifest.window_html
$expected_window_html = if ($resolved_typora_root) { Join-Path $resolved_typora_root "resources\window.html" } else { $null }
$expected_bundle_target = Join-Path (get_typora_windows_user_data) "linux_note_enhancements\typora_enhancements.js"
if (-not $resolved_typora_root -or
    $manifest.window_html -ne $expected_window_html -or
    $manifest.bundle_target -ne $expected_bundle_target) {
    throw "Backup targets do not match the current validated Typora paths; restore stopped."
}
if ($manifest.bundle_backup -and -not (Test-Path -LiteralPath $manifest.bundle_backup -PathType Leaf)) {
    throw "Recorded bundle backup is missing: $($manifest.bundle_backup)"
}

$safety_backup = Join-Path $backup_root "window.before_restore.$(Get-Date -Format 'yyyyMMdd-HHmmss').html"
Copy-Item -LiteralPath $manifest.window_html -Destination $safety_backup
Copy-Item -LiteralPath $window_backup -Destination $manifest.window_html -Force

if ($manifest.bundle_backup -and (Test-Path -LiteralPath $manifest.bundle_backup -PathType Leaf)) {
    Copy-Item -LiteralPath $manifest.bundle_backup -Destination $manifest.bundle_target -Force
} elseif (Test-Path -LiteralPath $manifest.bundle_target -PathType Leaf) {
    Rename-Item -LiteralPath $manifest.bundle_target -NewName "typora_enhancements.js.disabled.$(Get-Date -Format 'yyyyMMdd-HHmmss')"
}

Write-Host "Typora entry restored from: $window_backup"
Write-Host "Pre-restore safety copy: $safety_backup"
Write-Host "Restart Typora after saving open documents."
