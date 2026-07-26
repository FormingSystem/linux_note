param(
    [Parameter(Mandatory = $true)]
    [string]$PracticeDir,
    [int]$RequiredMajor = 18
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
New-Item -ItemType Directory -Force -Path $runtimeRoot | Out-Null

foreach ($majorText in $preferredMajors) {
    $major = 0
    if (-not [int]::TryParse($majorText, [ref]$major) -or $major -lt $RequiredMajor) {
        continue
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

        $expectedHash = $match.Groups[1].Value.ToUpperInvariant()
        $archiveName = $match.Groups[2].Value
        $archivePath = Join-Path $attemptDir $archiveName
        Write-Host "[practice] Downloading $archiveName..."
        Invoke-WebRequest -UseBasicParsing -TimeoutSec 180 -Uri "$releaseBase/$archiveName" -OutFile $archivePath

        $actualHash = (Get-FileHash -Algorithm SHA256 $archivePath).Hash
        if ($actualHash -ne $expectedHash) {
            throw "SHA-256 verification failed for $archiveName"
        }

        $expandedDir = Join-Path $attemptDir "expanded"
        Expand-Archive -LiteralPath $archivePath -DestinationPath $expandedDir
        $nodeHome = Get-ChildItem -LiteralPath $expandedDir -Directory | Select-Object -First 1
        if (-not $nodeHome -or
            -not (Test-Path (Join-Path $nodeHome.FullName "node.exe")) -or
            -not (Test-Path (Join-Path $nodeHome.FullName "npm.cmd"))) {
            throw "The downloaded Node.js archive is incomplete"
        }

        if (Test-Path $targetDir) {
            Remove-Item -LiteralPath $targetDir -Recurse -Force
        }
        Move-Item -LiteralPath $nodeHome.FullName -Destination $targetDir
        $installedVersion = & (Join-Path $targetDir "node.exe") --version
        Write-Host "[practice] Installed $installedVersion in the tool-local runtime."
        Remove-Item -LiteralPath $attemptDir -Recurse -Force
        exit 0
    } catch {
        Write-Warning "[practice] Node.js v$major failed: $($_.Exception.Message)"
        if (Test-Path $attemptDir) {
            Remove-Item -LiteralPath $attemptDir -Recurse -Force
        }
    }
}

Write-Error "[practice] No compatible official Node.js release could be downloaded."
exit 1
