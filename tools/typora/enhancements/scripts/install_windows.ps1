[CmdletBinding()]
param(
    [string]$typora_root = "",
    [string]$backup_root = "",
    [switch]$non_interactive
)

$ErrorActionPreference = "Stop"

function Get-Sha256([string]$path) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        return $null
    }
    return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
}

$typora_tools_root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$environment_helper = Join-Path $typora_tools_root "scripts\lib\typora_environment.ps1"
if (-not (Test-Path -LiteralPath $environment_helper -PathType Leaf)) {
    throw "Typora environment helper is missing: $environment_helper"
}
. $environment_helper
$typora_root = resolve_typora_windows_root -typora_root $typora_root -non_interactive:$non_interactive
$window_html = Join-Path $typora_root "resources\window.html"
$bundle_source = Join-Path (Split-Path -Parent $PSScriptRoot) "dist\typora_enhancements.js"
$user_data = get_typora_windows_user_data
$extension_target = Join-Path $user_data "linux_note_enhancements"
$bundle_target = Join-Path $extension_target "typora_enhancements.js"
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
if ([string]::IsNullOrWhiteSpace($backup_root)) {
    $backup_root = Join-Path $user_data "backups\linux_note_typora_enhancements\$timestamp"
}
$window_backup = Join-Path $backup_root "window.html"
$bundle_backup = Join-Path $backup_root "typora_enhancements.js"
$manifest_path = Join-Path $backup_root "manifest.json"
$script_tag = '<script defer src="typora://app/userData/linux_note_enhancements/typora_enhancements.js" data-linux-note-enhancements="true"></script>'

foreach ($required in @($window_html, $bundle_source)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required file is missing: $required"
    }
}

New-Item -ItemType Directory -Force -Path $backup_root, $extension_target | Out-Null
Copy-Item -LiteralPath $window_html -Destination $window_backup
if (Test-Path -LiteralPath $bundle_target -PathType Leaf) {
    Copy-Item -LiteralPath $bundle_target -Destination $bundle_backup
}

$window_before_hash = Get-Sha256 $window_html
$bundle_before_hash = Get-Sha256 $bundle_target
$window_source = [System.IO.File]::ReadAllText($window_html)
$tag_pattern = '<script\s+defer\s+src="typora://app/userData/linux_note_enhancements/typora_enhancements\.js"\s+data-linux-note-enhancements="true"></script>'
$window_source = [regex]::Replace($window_source, $tag_pattern, "")
if (-not $window_source.Contains("</body>")) {
    throw "Typora resources/window.html does not contain </body>; installation stopped before overwrite."
}
$window_source = $window_source.Replace("</body>", "$script_tag</body>")

try {
    Copy-Item -LiteralPath $bundle_source -Destination $bundle_target -Force
    [System.IO.File]::WriteAllText($window_html, $window_source, [System.Text.UTF8Encoding]::new($false))

    $installed_source = [System.IO.File]::ReadAllText($window_html)
    $installed_tag_count = ([regex]::Matches($installed_source, [regex]::Escape($script_tag))).Count
    if ($installed_tag_count -ne 1) {
        throw "Expected one enhancement script tag after installation, found $installed_tag_count."
    }

    $manifest = [ordered]@{
        installed_at = (Get-Date).ToString("o")
        typora_root = $typora_root
        typora_version = get_typora_windows_version $typora_root
        window_html = $window_html
        window_backup = $window_backup
        window_before_sha256 = $window_before_hash
        window_after_sha256 = Get-Sha256 $window_html
        bundle_target = $bundle_target
        bundle_backup = if (Test-Path -LiteralPath $bundle_backup) { $bundle_backup } else { $null }
        bundle_before_sha256 = $bundle_before_hash
        bundle_after_sha256 = Get-Sha256 $bundle_target
    }
    [System.IO.File]::WriteAllText(
        $manifest_path,
        ($manifest | ConvertTo-Json -Depth 4),
        [System.Text.UTF8Encoding]::new($false)
    )
} catch {
    Copy-Item -LiteralPath $window_backup -Destination $window_html -Force
    if (Test-Path -LiteralPath $bundle_backup -PathType Leaf) {
        Copy-Item -LiteralPath $bundle_backup -Destination $bundle_target -Force
    } elseif (Test-Path -LiteralPath $bundle_target -PathType Leaf) {
        Rename-Item -LiteralPath $bundle_target -NewName "typora_enhancements.js.disabled.$timestamp"
    }
    throw
}

Write-Host "Typora enhancements installed."
Write-Host "Typora version: $($manifest.typora_version)"
Write-Host "Typora root: $typora_root"
Write-Host "Bundle: $bundle_target"
Write-Host "Backup: $backup_root"
Write-Host "Restart Typora after saving open documents."
