Set-StrictMode -Version Latest

function convert_typora_input_path {
    param(
        [Parameter(Mandatory = $true)]
        [string]$input_path
    )

    $normalized_path = [Environment]::ExpandEnvironmentVariables(
        $input_path.Trim().Trim('"').Trim("'")
    )
    if ($normalized_path -match '^/mnt/([A-Za-z])(?:/(.*))?$') {
        $drive_letter = $Matches[1].ToUpperInvariant()
        $path_tail = if ($Matches.Count -gt 2) { $Matches[2] -replace '/', '\' } else { "" }
        if ($path_tail) {
            return "${drive_letter}:\$path_tail"
        }
        return "${drive_letter}:\"
    }
    if ($normalized_path -match '^/([A-Za-z])(?:/(.*))?$') {
        $drive_letter = $Matches[1].ToUpperInvariant()
        $path_tail = if ($Matches.Count -gt 2) { $Matches[2] -replace '/', '\' } else { "" }
        if ($path_tail) {
            return "${drive_letter}:\$path_tail"
        }
        return "${drive_letter}:\"
    }
    return $normalized_path -replace '/', '\'
}

function get_typora_root_from_candidate {
    param(
        [Parameter(Mandatory = $true)]
        [string]$candidate_path
    )

    if ([string]::IsNullOrWhiteSpace($candidate_path)) {
        return $null
    }
    $candidate_path = convert_typora_input_path $candidate_path
    if (-not (Test-Path -LiteralPath $candidate_path)) {
        return $null
    }

    $resolved_candidate = (Resolve-Path -LiteralPath $candidate_path).Path
    $candidate_item = Get-Item -LiteralPath $resolved_candidate
    $candidate_root = $null
    if (-not $candidate_item.PSIsContainer) {
        if ($candidate_item.Name -ieq "window.html" -and
            (Split-Path -Leaf (Split-Path -Parent $candidate_item.FullName)) -ieq "resources") {
            $candidate_root = Split-Path -Parent (Split-Path -Parent $candidate_item.FullName)
        } elseif ($candidate_item.Name -ieq "Typora.exe") {
            $candidate_root = Split-Path -Parent $candidate_item.FullName
        }
    } elseif (Test-Path -LiteralPath (Join-Path $candidate_item.FullName "resources\window.html") -PathType Leaf) {
        $candidate_root = $candidate_item.FullName
    } elseif ($candidate_item.Name -ieq "resources" -and
        (Test-Path -LiteralPath (Join-Path $candidate_item.FullName "window.html") -PathType Leaf)) {
        $candidate_root = Split-Path -Parent $candidate_item.FullName
    }

    if (-not $candidate_root) {
        return $null
    }
    $candidate_root = (Resolve-Path -LiteralPath $candidate_root).Path
    foreach ($required_relative_path in @("Typora.exe", "resources\window.html")) {
        if (-not (Test-Path -LiteralPath (Join-Path $candidate_root $required_relative_path) -PathType Leaf)) {
            return $null
        }
    }
    return $candidate_root
}

function get_typora_discovery_candidates {
    $candidate_paths = [System.Collections.Generic.List[string]]::new()

    if (-not [string]::IsNullOrWhiteSpace($env:TYPORA_ROOT)) {
        $candidate_paths.Add($env:TYPORA_ROOT)
    }
    Get-Process Typora -ErrorAction SilentlyContinue |
        Where-Object { $_.Path } |
        ForEach-Object { $candidate_paths.Add($_.Path) }
    foreach ($command_name in @("Typora.exe", "typora")) {
        $command = Get-Command $command_name -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($command -and $command.Source) {
            $candidate_paths.Add($command.Source)
        }
    }

    foreach ($registry_path in @(
        "Registry::HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\App Paths\Typora.exe",
        "Registry::HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\App Paths\Typora.exe"
    )) {
        $registry_item = Get-ItemProperty -LiteralPath $registry_path -ErrorAction SilentlyContinue
        $default_property = if ($registry_item) { $registry_item.PSObject.Properties['(default)'] } else { $null }
        if ($default_property -and $default_property.Value) {
            $candidate_paths.Add([string]$default_property.Value)
        }
    }

    return $candidate_paths
}

function resolve_typora_windows_root {
    param(
        [string]$typora_root = "",
        [switch]$non_interactive
    )

    if (-not [string]::IsNullOrWhiteSpace($typora_root)) {
        $resolved_root = get_typora_root_from_candidate $typora_root
        if (-not $resolved_root) {
            throw "The supplied Typora path is not an installation root, executable, resources directory, or resources/window.html: $typora_root"
        }
        return $resolved_root
    }

    foreach ($candidate_path in get_typora_discovery_candidates) {
        $resolved_root = get_typora_root_from_candidate $candidate_path
        if ($resolved_root) {
            return $resolved_root
        }
    }

    if ($non_interactive) {
        throw "Typora was not discovered. Pass -typora_root or set TYPORA_ROOT."
    }
    $requested_path = Read-Host "Typora was not discovered. Enter its installation directory, Typora.exe, resources directory, or resources/window.html"
    if ([string]::IsNullOrWhiteSpace($requested_path)) {
        throw "No Typora path was provided."
    }
    $resolved_root = get_typora_root_from_candidate $requested_path
    if (-not $resolved_root) {
        throw "The entered path does not contain a valid Typora installation: $requested_path"
    }
    return $resolved_root
}

function get_typora_windows_user_data {
    if ([string]::IsNullOrWhiteSpace($env:APPDATA)) {
        throw "APPDATA is unavailable; the Typora user-data directory cannot be resolved."
    }
    return Join-Path $env:APPDATA "Typora"
}

function get_typora_windows_version {
    param(
        [Parameter(Mandatory = $true)]
        [string]$typora_root
    )
    return (Get-Item -LiteralPath (Join-Path $typora_root "Typora.exe")).VersionInfo.ProductVersion
}
