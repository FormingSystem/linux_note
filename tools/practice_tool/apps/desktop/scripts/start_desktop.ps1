[CmdletBinding()]
param(
  [switch]$check_only
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function get_required_command {
  param(
    [Parameter(Mandatory = $true)]
    [string]$command_name,
    [Parameter(Mandatory = $true)]
    [string]$install_hint
  )

  $command = Get-Command $command_name -ErrorAction SilentlyContinue
  if ($null -eq $command) {
    throw "Missing $command_name. $install_hint"
  }
  return $command.Source
}

function invoke_checked {
  param(
    [Parameter(Mandatory = $true)]
    [string]$command_path,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$command_arguments
  )

  & $command_path @command_arguments
  if ($LASTEXITCODE -ne 0) {
    throw "Command failed with exit code $LASTEXITCODE`: $command_path $($command_arguments -join ' ')"
  }
}

function native_build_required {
  param(
    [Parameter(Mandatory = $true)]
    [string]$tool_root,
    [Parameter(Mandatory = $true)]
    [string]$native_executable
  )

  if (-not (Test-Path -LiteralPath $native_executable -PathType Leaf)) {
    return $true
  }

  $executable_time = (Get-Item -LiteralPath $native_executable).LastWriteTimeUtc
  $native_inputs = @(
    Get-Item -LiteralPath (Join-Path $tool_root "native\CMakeLists.txt")
    Get-Item -LiteralPath (Join-Path $tool_root "native\CMakePresets.json")
    Get-ChildItem -LiteralPath (Join-Path $tool_root "native\src") -Recurse -File
  )
  return $null -ne ($native_inputs | Where-Object { $_.LastWriteTimeUtc -gt $executable_time } | Select-Object -First 1)
}

try {
  $tool_root = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
  $node_command = get_required_command "node.exe" "Install Node.js 22.12 or newer and add it to PATH."
  $npm_command = get_required_command "npm.cmd" "Install Node.js 22.12 or newer with npm."

  $node_version_text = (& $node_command --version).Trim().TrimStart("v")
  $node_version = [version]$node_version_text
  if ($node_version -lt [version]"22.12.0") {
    throw "Node.js v$node_version_text is too old. Loop desktop requires v22.12.0 or newer."
  }

  Write-Host "[Loop] Tool root: $tool_root" -ForegroundColor Cyan
  Write-Host "[Loop] Node.js: v$node_version_text"

  $lock_file = Join-Path $tool_root "package-lock.json"
  $installed_lock = Join-Path $tool_root "node_modules\.package-lock.json"
  $dependencies_required = -not (Test-Path -LiteralPath $installed_lock -PathType Leaf)
  if (-not $dependencies_required) {
    $dependencies_required = (Get-Item -LiteralPath $lock_file).LastWriteTimeUtc `
        -gt (Get-Item -LiteralPath $installed_lock).LastWriteTimeUtc
  }
  if ($dependencies_required) {
    Write-Host "[Loop] Installing npm dependencies from the workspace lockfile..." -ForegroundColor Yellow
    Push-Location $tool_root
    try {
      invoke_checked $npm_command install
    } finally {
      Pop-Location
    }
  } else {
    Write-Host "[Loop] npm dependencies are ready."
  }

  $native_root = Join-Path $tool_root "native"
  $native_executable = Join-Path $native_root "build\windows-mingw\loop_native_service.exe"
  if (native_build_required $tool_root $native_executable) {
    $cmake_command = get_required_command "cmake.exe" "Install CMake 3.22 or newer and add it to PATH."
    [void](get_required_command "gcc.exe" "Install MinGW GCC and add it to PATH.")
    [void](get_required_command "mingw32-make.exe" "Install MinGW Make and add it to PATH.")
    Write-Host "[Loop] Configuring and building the C++ Native Service..." -ForegroundColor Yellow
    Push-Location $native_root
    try {
      invoke_checked $cmake_command --preset windows-mingw
      invoke_checked $cmake_command --build --preset windows-mingw
    } finally {
      Pop-Location
    }
  } else {
    Write-Host "[Loop] C++ Native Service is ready."
  }

  if ($check_only) {
    Write-Host "[Loop] One-click startup check passed." -ForegroundColor Green
    exit 0
  }

  Write-Host "[Loop] Starting the Loop Markdown workbench..." -ForegroundColor Green
  Push-Location $tool_root
  try {
    invoke_checked $npm_command run desktop:start
  } finally {
    Pop-Location
  }
} catch {
  Write-Host "[Loop] Startup failed: $($_.Exception.Message)" -ForegroundColor Red
  exit 1
}
