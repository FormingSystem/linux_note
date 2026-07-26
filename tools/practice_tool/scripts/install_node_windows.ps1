param(
    [Parameter(Mandatory = $true)]
    [string]$PracticeDir,
    [int]$RequiredMajor = 18,
    [ValidateSet("auto", "manual", "table")]
    [string]$Mode = "auto"
)

$ErrorActionPreference = "Stop"
$preferredMajors = if ($env:PRACTICE_NODE_MAJORS) {
    $env:PRACTICE_NODE_MAJORS -split "\s+"
} else {
    @("24", "22", "20", "18")
}

$architecture = switch ($env:PROCESSOR_ARCHITECTURE) {
    "AMD64" { "x64" }
    "ARM64" { "arm64" }
    default { throw "Unsupported Windows architecture: $env:PROCESSOR_ARCHITECTURE" }
}

$runtimeRoot = Join-Path $PracticeDir ".local\runtime"
$targetDir = Join-Path $runtimeRoot "node"
$cacheRoot = Join-Path $PracticeDir ".local\downloads\node"
New-Item -ItemType Directory -Force -Path $runtimeRoot | Out-Null
New-Item -ItemType Directory -Force -Path $cacheRoot | Out-Null

function Install-NodeArchive {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PackageDir,
        [Parameter(Mandatory = $true)]
        [string]$ArchiveName,
        [string]$ChecksumsPath
    )

    if (-not $ChecksumsPath) {
        $ChecksumsPath = Join-Path $PackageDir "SHASUMS256.txt"
    }
    $archivePath = Join-Path $PackageDir $ArchiveName
    if (-not (Test-Path $ChecksumsPath) -or -not (Test-Path $archivePath)) {
        return $false
    }
    $namePattern = "^node-v(?<major>[0-9]+)\.[0-9]+\.[0-9]+-win-$architecture\.(?<suffix>zip|7z)$"
    $nameMatch = [regex]::Match($ArchiveName, $namePattern)
    if (-not $nameMatch -or [int]$nameMatch.Groups["major"].Value -lt $RequiredMajor) {
        Write-Warning "[practice] Package name/version/platform is incompatible: $ArchiveName"
        return $false
    }

    $pattern = "^([0-9a-fA-F]{64})  $([regex]::Escape($ArchiveName))$"
    $match = Get-Content $ChecksumsPath |
        ForEach-Object { [regex]::Match($_, $pattern) } |
        Where-Object Success |
        Select-Object -First 1
    if (-not $match) {
        return $false
    }
    $expectedHash = $match.Groups[1].Value.ToUpperInvariant()
    $actualHash = (Get-FileHash -Algorithm SHA256 $archivePath).Hash
    if ($actualHash -ne $expectedHash) {
        Write-Warning "[practice] Offline cache verification failed: $archivePath"
        return $false
    }

    $expandedDir = Join-Path $runtimeRoot ("node-expanded." + [guid]::NewGuid().ToString("N"))
    if ($nameMatch.Groups["suffix"].Value -eq "zip") {
        Expand-Archive -LiteralPath $archivePath -DestinationPath $expandedDir
    } else {
        $sevenZip = Get-Command 7z.exe, 7za.exe -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $sevenZip) {
            Write-Warning "[practice] Extracting .7z requires 7z.exe or 7za.exe."
            return $false
        }
        New-Item -ItemType Directory -Force -Path $expandedDir | Out-Null
        & $sevenZip.Source x "-o$expandedDir" -y $archivePath | Out-Null
        if ($LASTEXITCODE -ne 0) {
            return $false
        }
    }
    $nodeHome = Get-ChildItem -LiteralPath $expandedDir -Directory | Select-Object -First 1
    if (-not $nodeHome -or
        -not (Test-Path (Join-Path $nodeHome.FullName "node.exe")) -or
        -not (Test-Path (Join-Path $nodeHome.FullName "npm.cmd"))) {
        Remove-Item -LiteralPath $expandedDir -Recurse -Force
        return $false
    }

    if (Test-Path $targetDir) {
        Remove-Item -LiteralPath $targetDir -Recurse -Force
    }
    Move-Item -LiteralPath $nodeHome.FullName -Destination $targetDir
    Remove-Item -LiteralPath $expandedDir -Recurse -Force
    $installedVersion = & (Join-Path $targetDir "node.exe") --version
    Write-Host "[practice] Installed $installedVersion from cache."
    return $true
}

function Resolve-PracticePath {
    param([string]$Value)
    if ([System.IO.Path]::IsPathRooted($Value)) {
        return [System.IO.Path]::GetFullPath($Value)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $PracticeDir $Value))
}

function Try-InstallNodeArchive {
    param(
        [string]$PackageDir,
        [string]$ArchiveName,
        [string]$ChecksumsPath
    )
    try {
        return Install-NodeArchive -PackageDir $PackageDir -ArchiveName $ArchiveName -ChecksumsPath $ChecksumsPath
    } catch {
        Write-Warning "[practice] Offline package failed: $($_.Exception.Message)"
        return $false
    }
}

function Install-FromTable {
    param([string]$TablePath)
    if (-not (Test-Path $TablePath)) {
        $tableDirectory = Split-Path $TablePath
        if ($tableDirectory) {
            New-Item -ItemType Directory -Force -Path $tableDirectory | Out-Null
        }
        Copy-Item -LiteralPath (Join-Path $PracticeDir "config\offline_node_packages.example.tsv") -Destination $TablePath
        Write-Host "[practice] Created offline package table: $TablePath"
        Write-Host "[practice] Set enabled to 1, fill relative-to-practice-root or absolute paths, then run again."
        return $false
    }

    $rows = Import-Csv -LiteralPath $TablePath -Delimiter "`t"
    foreach ($row in $rows) {
        if ($row.enabled -ne "1" -or $row.platform -ne "windows" -or $row.arch -ne $architecture) {
            continue
        }
        $archivePath = Resolve-PracticePath $row.archive
        $checksumsPath = Resolve-PracticePath $row.checksums
        if (Try-InstallNodeArchive -PackageDir (Split-Path $archivePath) -ArchiveName (Split-Path $archivePath -Leaf) -ChecksumsPath $checksumsPath) {
            return $true
        }
        Write-Warning "[practice] Table item failed: $archivePath | $checksumsPath"
    }
    return $false
}

if ($Mode -eq "manual") {
    $manualArchive = Resolve-PracticePath (Read-Host "[practice] Archive path (relative to practice root or absolute)")
    $manualChecksums = Resolve-PracticePath (Read-Host "[practice] SHASUMS256.txt path")
    $manualDir = Split-Path $manualArchive
    if (Try-InstallNodeArchive -PackageDir $manualDir -ArchiveName (Split-Path $manualArchive -Leaf) -ChecksumsPath $manualChecksums) {
        exit 0
    }
    Write-Warning "[practice] Manual offline package failed; continuing with automatic installation."
} elseif ($Mode -eq "table") {
    $tablePath = if ($env:PRACTICE_OFFLINE_NODE_TABLE) {
        Resolve-PracticePath $env:PRACTICE_OFFLINE_NODE_TABLE
    } else {
        Join-Path $PracticeDir "config\offline_node_packages.local.tsv"
    }
    if (Install-FromTable $tablePath) {
        exit 0
    }
    Write-Warning "[practice] No usable table entry; continuing with automatic installation."
}

foreach ($majorText in $preferredMajors) {
    $major = 0
    if (-not [int]::TryParse($majorText, [ref]$major) -or $major -lt $RequiredMajor) {
        continue
    }

    $cachedPackages = Get-ChildItem -LiteralPath $cacheRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like "v$major.*" } |
        Sort-Object Name -Descending
    foreach ($cachedPackage in $cachedPackages) {
        $cachedArchive = Get-ChildItem -LiteralPath $cachedPackage.FullName -File |
            Where-Object { $_.Name -match "^node-v.*-win-$architecture\.(zip|7z)$" } |
            Select-Object -First 1
        if ($cachedArchive) {
            Write-Host "[practice] Found offline cache: $($cachedArchive.FullName)"
            if (Try-InstallNodeArchive -PackageDir $cachedPackage.FullName -ArchiveName $cachedArchive.Name) {
                exit 0
            }
        }
    }

    $attemptDir = Join-Path $runtimeRoot ("node-v{0}.{1}" -f $major, [guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $attemptDir | Out-Null
    try {
        $releaseBase = "https://nodejs.org/dist/latest-v$major.x"
        $checksumsPath = Join-Path $attemptDir "SHASUMS256.txt"
        Write-Host "[practice] Querying official Node.js latest-v$major.x (win-$architecture)..."
        Invoke-WebRequest -UseBasicParsing -TimeoutSec 60 -Uri "$releaseBase/SHASUMS256.txt" -OutFile $checksumsPath

        $pattern = "^([0-9a-fA-F]{64})  (node-v[^ ]+-win-$architecture\.zip)$"
        $match = Get-Content $checksumsPath |
            ForEach-Object { [regex]::Match($_, $pattern) } |
            Where-Object Success |
            Select-Object -First 1
        if (-not $match) {
            throw "No official win-$architecture archive in latest-v$major.x"
        }

        $archiveName = $match.Groups[2].Value
        $version = ($archiveName -replace "^node-v", "") -replace "-win-.*$", ""
        $packageDir = Join-Path $cacheRoot "v$version"
        New-Item -ItemType Directory -Force -Path $packageDir | Out-Null
        Copy-Item -LiteralPath $checksumsPath -Destination (Join-Path $packageDir "SHASUMS256.txt") -Force
        $archivePath = Join-Path $packageDir $archiveName
        Write-Host "[practice] Downloading $archiveName..."
        Invoke-WebRequest -UseBasicParsing -TimeoutSec 180 -Uri "$releaseBase/$archiveName" -OutFile $archivePath

        if (-not (Install-NodeArchive -PackageDir $packageDir -ArchiveName $archiveName)) {
            throw "SHA-256 verification or extraction failed for $archiveName"
        }
        Remove-Item -LiteralPath $attemptDir -Recurse -Force
        exit 0
    } catch {
        Write-Warning "[practice] Node.js v$major failed: $($_.Exception.Message)"
        if (Test-Path $attemptDir) {
            Remove-Item -LiteralPath $attemptDir -Recurse -Force
        }
    }
}

Write-Host "[practice] Offline use: place the official ZIP and matching SHASUMS256.txt under:"
Write-Host "[practice]   $cacheRoot\v<version>\"
Write-Host "[practice] Example: $cacheRoot\v24.18.0\node-v24.18.0-win-$architecture.zip"
Write-Error "[practice] No compatible official or cached Node.js release is available."
exit 1
