param(
    [string]$InstallRoot = "C:\msys64"
)

$ErrorActionPreference = "Stop"
$practiceDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$shellLauncher = Join-Path $InstallRoot "msys2_shell.cmd"

if (Test-Path $shellLauncher) {
    Write-Host "[practice] MSYS2 is already installed: $InstallRoot"
} else {
    $downloadDir = Join-Path $practiceDir ".local\downloads\msys2"
    New-Item -ItemType Directory -Force -Path $downloadDir | Out-Null
    $installerPath = Join-Path $downloadDir "msys2-x86_64-latest.exe"
    $checksumPath = "$installerPath.sha256"
    $sources = if ($env:PRACTICE_MSYS2_INSTALLER_SOURCES) {
        $env:PRACTICE_MSYS2_INSTALLER_SOURCES -split "\s+"
    } else {
        @(
            "https://mirrors.tuna.tsinghua.edu.cn/msys2/distrib/x86_64",
            "https://repo.msys2.org/distrib/x86_64"
        )
    }

    $downloaded = $false
    foreach ($source in $sources) {
        $baseUrl = $source.TrimEnd("/")
        try {
            Write-Host "[practice] Trying MSYS2 installer source: $baseUrl"
            Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/msys2-x86_64-latest.exe.sha256" -OutFile $checksumPath
            Invoke-WebRequest -UseBasicParsing -Uri "$baseUrl/msys2-x86_64-latest.exe" -OutFile $installerPath
            $expected = ((Get-Content -LiteralPath $checksumPath -Raw) -split "\s+")[0].ToUpperInvariant()
            $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $installerPath).Hash
            if ($expected -ne $actual) {
                throw "SHA-256 verification failed"
            }
            $downloaded = $true
            break
        } catch {
            Write-Warning "[practice] MSYS2 source failed: $($_.Exception.Message)"
            Remove-Item -LiteralPath $installerPath, $checksumPath -Force -ErrorAction SilentlyContinue
        }
    }
    if (-not $downloaded) {
        throw "Unable to download a verified MSYS2 installer from configured sources."
    }

    $rootArgument = $InstallRoot.Replace("\", "/")
    Write-Host "[practice] Installing official MSYS2 to $InstallRoot ..."
    $process = Start-Process -FilePath $installerPath -ArgumentList @(
        "in",
        "--confirm-command",
        "--accept-messages",
        "--root",
        $rootArgument
    ) -Wait -PassThru -WindowStyle Hidden
    if ($process.ExitCode -ne 0 -or -not (Test-Path $shellLauncher)) {
        throw "MSYS2 installer failed with exit code $($process.ExitCode)."
    }
}

Write-Host ""
Write-Host "[practice] Windows bootstrap is complete."
Write-Host "[practice] Recommended official environment: MSYS2 UCRT64."
Write-Host "[practice] Double-click this shortcut to run the training tool:"
Write-Host "[practice]   $(Join-Path $practiceDir 'start_ucrt64.cmd')"
Write-Host "[practice] Or open UCRT64 and run:"
Write-Host "[practice]   cd <practice_tool path>"
Write-Host "[practice]   ./start.sh"
Write-Host "[practice] UCRT32 is supported only when an existing/custom UCRT32 environment is present;"
Write-Host "[practice] current official MSYS2 installers provide UCRT64, not UCRT32."
