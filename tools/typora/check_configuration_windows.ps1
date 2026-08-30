[CmdletBinding()]
param(
    [string]$typora_root = "",
    [switch]$non_interactive
)

$ErrorActionPreference = "Stop"

$environment_helper = Join-Path $PSScriptRoot "scripts\lib\typora_environment.ps1"
if (-not (Test-Path -LiteralPath $environment_helper -PathType Leaf)) {
    throw "Typora environment helper is missing: $environment_helper"
}
. $environment_helper
$typora_root = resolve_typora_windows_root -typora_root $typora_root -non_interactive:$non_interactive
$window_html = Join-Path $typora_root "resources\window.html"
$user_data = get_typora_windows_user_data
$theme = Join-Path $user_data "themes\cpp_github-consolas.css"
$bundle = Join-Path $user_data "linux_note_enhancements\typora_enhancements.js"

foreach ($required in @($window_html, $theme, $bundle)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Installed configuration file is missing: $required"
    }
}

$window_source = [System.IO.File]::ReadAllText($window_html)
$tag_count = ([regex]::Matches($window_source, 'data-linux-note-enhancements="true"')).Count
if ($tag_count -ne 1) {
    throw "Expected exactly one Typora enhancement entry, found $tag_count"
}

$bundle_source = [System.IO.File]::ReadAllText($bundle)
foreach ($marker in @(
    "linux-note-vscode-textmate-c",
    "linux-note-vscode-textmate-cpp",
    "linux-note-mermaid-viewer",
    "linux-note-code-collapsible",
    "linux-note-code-toggle",
    "is-code-collapsed",
    "mermaid_container_for_preview",
    "preview.prepend(toolbar)",
    "fit-width"
)) {
    if (-not $bundle_source.Contains($marker)) {
        throw "Installed bundle failed validation; missing marker: $marker"
    }
}

[pscustomobject]@{
    typora_version = get_typora_windows_version $typora_root
    enhancement_entries = $tag_count
    theme_sha256 = (Get-FileHash -LiteralPath $theme -Algorithm SHA256).Hash
    bundle_sha256 = (Get-FileHash -LiteralPath $bundle -Algorithm SHA256).Hash
    status = "OK"
} | Format-List
