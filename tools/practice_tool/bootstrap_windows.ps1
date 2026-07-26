param(
    [string]$InstallRoot = "C:\msys64"
)

$ErrorActionPreference = "Stop"
$practiceDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$shellLauncher = Join-Path $InstallRoot "msys2_shell.cmd"
$registryPath = Join-Path $practiceDir ".local\software_registry.tsv"

function Add-PracticeRegistryRecord {
    param(
        [string]$SoftwareId,
        [string]$VersionBefore,
        [string]$VersionAfter,
        [string]$ManagedPath,
        [ValidateSet("tool-owned", "external", "external-updated")]
        [string]$Ownership,
        [ValidateSet("msys-root", "download-cache", "none")]
        [string]$CleanupKind,
        [string]$Source
    )
    if ($Ownership -ne "tool-owned" -and $CleanupKind -ne "none") {
        throw "External software cannot be registered for cleanup."
    }
    $registryDir = Split-Path -Parent $registryPath
    New-Item -ItemType Directory -Force -Path $registryDir | Out-Null
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    if (-not (Test-Path -LiteralPath $registryPath)) {
        [IO.File]::WriteAllText(
            $registryPath,
            "recorded_at`tsoftware_id`tversion_before`tversion_after`tmanaged_path`townership`tcleanup_kind`tsource`n",
            $utf8
        )
    }
    $values = @(
        [DateTime]::UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ"),
        $SoftwareId, $VersionBefore, $VersionAfter, $ManagedPath,
        $Ownership, $CleanupKind, $Source
    ) | ForEach-Object { ([string]$_) -replace "[`t`r`n]", " " }
    [IO.File]::AppendAllText($registryPath, (($values -join "`t") + "`n"), $utf8)
}

if (Test-Path $shellLauncher) {
    Write-Host "[practice] MSYS2 is already installed: $InstallRoot"
    Add-PracticeRegistryRecord "msys2-root" "" "" $InstallRoot "external" "none" "pre-existing"
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
    Add-PracticeRegistryRecord "msys2-installer-cache" "" "latest" $downloadDir "tool-owned" "download-cache" $installerPath
    Add-PracticeRegistryRecord "msys2-root" "" "latest" $InstallRoot "tool-owned" "msys-root" $installerPath
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
