[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$backup_root
)

$ErrorActionPreference = "Stop"
$environment_helper = Join-Path $PSScriptRoot "scripts\lib\typora_environment.ps1"
if (-not (Test-Path -LiteralPath $environment_helper -PathType Leaf)) {
    throw "Typora environment helper is missing: $environment_helper"
}
. $environment_helper
$backup_root = convert_typora_input_path $backup_root
$backup_root = (Resolve-Path -LiteralPath $backup_root).Path
$configuration_manifest = Join-Path $backup_root "configuration_manifest.json"
$enhancement_restorer = Join-Path $PSScriptRoot "enhancements\scripts\restore_windows.ps1"

foreach ($required in @($configuration_manifest, $enhancement_restorer)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required restore file is missing: $required"
    }
}

$manifest = Get-Content -LiteralPath $configuration_manifest -Raw | ConvertFrom-Json
$expected_theme_target = Join-Path (get_typora_windows_user_data) "themes\cpp_github-consolas.css"
if ($manifest.theme_target -ne $expected_theme_target) {
    throw "Backup theme target does not match the current Typora user-data path; restore stopped."
}
if ($manifest.theme_existed -and -not (Test-Path -LiteralPath $manifest.theme_backup -PathType Leaf)) {
    throw "Recorded theme backup is missing: $($manifest.theme_backup)"
}
& $enhancement_restorer -backup_root $backup_root

if ($manifest.theme_existed) {
    if (-not (Test-Path -LiteralPath $manifest.theme_backup -PathType Leaf)) {
        throw "Theme backup is missing: $($manifest.theme_backup)"
    }
    Copy-Item -LiteralPath $manifest.theme_backup -Destination $manifest.theme_target -Force
} elseif (Test-Path -LiteralPath $manifest.theme_target -PathType Leaf) {
    $disabled_name = "cpp_github-consolas.css.disabled.$(Get-Date -Format 'yyyyMMdd-HHmmss')"
    Rename-Item -LiteralPath $manifest.theme_target -NewName $disabled_name
}

Write-Host "linux-note Typora configuration restored."
Write-Host "Backup source: $backup_root"
Write-Host "Save open documents and restart Typora."
