[CmdletBinding()]
param(
    [string]$typora_root = "",
    [switch]$non_interactive
)

$ErrorActionPreference = "Stop"

function Get-Sha256([string]$path) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        return $null
    }
    return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
}

$typora_tools_root = $PSScriptRoot
$environment_helper = Join-Path $typora_tools_root "scripts\lib\typora_environment.ps1"
if (-not (Test-Path -LiteralPath $environment_helper -PathType Leaf)) {
    throw "Typora environment helper is missing: $environment_helper"
}
. $environment_helper
$typora_root = resolve_typora_windows_root -typora_root $typora_root -non_interactive:$non_interactive
$enhancement_root = Join-Path $typora_tools_root "enhancements"
$bundle_source = Join-Path $enhancement_root "dist\typora_enhancements.js"
$theme_source = Join-Path $typora_tools_root "cpp_github-consolas.css"
$installer = Join-Path $enhancement_root "scripts\install_windows.ps1"
$user_data = get_typora_windows_user_data
$theme_directory = Join-Path $user_data "themes"
$theme_target = Join-Path $theme_directory "cpp_github-consolas.css"
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$backup_root = Join-Path $user_data "backups\linux_note_typora_configuration\$timestamp"
$theme_backup = Join-Path $backup_root "cpp_github-consolas.css"
$configuration_manifest = Join-Path $backup_root "configuration_manifest.json"

foreach ($required in @($bundle_source, $theme_source, $installer)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required configuration file is missing: $required"
    }
}

$bundle_text = [System.IO.File]::ReadAllText($bundle_source)
foreach ($marker in @(
    "linux-note-vscode-textmate-c",
    "linux-note-vscode-textmate-cpp",
    "linux-note-mermaid-viewer",
    "linux-note-code-collapsible",
    "linux-note-code-toggle",
    "is-code-collapsed",
    "mermaid_container_for_preview",
    "preview.prepend(toolbar)",
    "data-linux-note-typora-enhancements"
)) {
    if (-not $bundle_text.Contains($marker)) {
        throw "Prebuilt enhancement bundle failed validation; missing marker: $marker"
    }
}

New-Item -ItemType Directory -Force -Path $backup_root, $theme_directory | Out-Null
$theme_existed = Test-Path -LiteralPath $theme_target -PathType Leaf
$theme_before_hash = Get-Sha256 $theme_target
if ($theme_existed) {
    Copy-Item -LiteralPath $theme_target -Destination $theme_backup
}

Copy-Item -LiteralPath $theme_source -Destination $theme_target -Force

try {
    & $installer -typora_root $typora_root -backup_root $backup_root -non_interactive
} catch {
    if ($theme_existed -and (Test-Path -LiteralPath $theme_backup -PathType Leaf)) {
        Copy-Item -LiteralPath $theme_backup -Destination $theme_target -Force
    } elseif (Test-Path -LiteralPath $theme_target -PathType Leaf) {
        Rename-Item -LiteralPath $theme_target -NewName "cpp_github-consolas.css.disabled.$timestamp"
    }
    throw
}

$manifest = [ordered]@{
    configured_at = (Get-Date).ToString("o")
    backup_root = $backup_root
    theme_source = $theme_source
    theme_target = $theme_target
    theme_existed = $theme_existed
    theme_backup = if ($theme_existed) { $theme_backup } else { $null }
    theme_before_sha256 = $theme_before_hash
    theme_after_sha256 = Get-Sha256 $theme_target
    enhancement_manifest = Join-Path $backup_root "manifest.json"
}
[System.IO.File]::WriteAllText(
    $configuration_manifest,
    ($manifest | ConvertTo-Json -Depth 4),
    [System.Text.UTF8Encoding]::new($false)
)

Write-Host ""
Write-Host "linux-note Typora configuration completed."
Write-Host "Theme: $theme_target"
Write-Host "Unified backup: $backup_root"
Write-Host "Save open documents, restart Typora, and select 'cpp github consolas' from the Theme menu."
