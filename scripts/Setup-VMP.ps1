#Requires -Version 7.0
<#
.SYNOPSIS
    Fully automates the VMP build setup: submodules, CEF, prebuild, project generation, and opening Visual Studio.
.DESCRIPTION
    Runs the documented `docs/building.md` steps in order:
      1. Optional prerequisite check.
      2. git submodule update --jobs=16 --init
      3. pip install setuptools
      4. fxd get-chrome
      5. prebuild
      6. fxd gen -game <Game>
      7. fxd vs -game <Game> (or fxd build)
    You can stop after any step with -StopAfter.
.PARAMETER Game
    Game target to generate/build. Default: five.
.PARAMETER SkipPrereqCheck
    Do not run Check-Prerequisites.ps1 first.
.PARAMETER StopAfter
    Stop after one of: Submodules, Setuptools, Chrome, Prebuild, Gen.
.PARAMETER BuildOnly
    Instead of opening Visual Studio, run `fxd build` for the chosen game.
.PARAMETER Configuration
    Build configuration to use with -BuildOnly. Default: Debug.
.EXAMPLE
    .\Setup-VMP.ps1
    .\Setup-VMP.ps1 -Game server -BuildOnly
    .\Setup-VMP.ps1 -StopAfter Gen
#>
[CmdletBinding()]
param(
    [ValidateSet('five', 'rdr3', 'ny', 'server')]
    [string]$Game = 'five',
    [switch]$SkipPrereqCheck,
    [ValidateSet('', 'Submodules', 'Setuptools', 'Chrome', 'Prebuild', 'Gen')]
    [string]$StopAfter = '',
    [switch]$BuildOnly,
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'

$RepoRoot = Resolve-Path "$PSScriptRoot\.."
$Fxd = "$RepoRoot\fxd.ps1"
$Prebuild = "$RepoRoot\prebuild.cmd"

function Write-Step($n, $msg) {
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "STEP $n`: $msg" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
}

function Write-Info($msg) { Write-Host "[INFO] $msg" -ForegroundColor Cyan }
function Write-Ok($msg) { Write-Host "[OK]   $msg" -ForegroundColor Green }

function Stop-IfRequested {
    param([string]$Step)
    if ($StopAfter -eq $Step) {
        Write-Host "`nStopped after $Step as requested." -ForegroundColor Yellow
        exit 0
    }
}

# ---------------------------------------------------------------------------
# 0. Prerequisite check
# ---------------------------------------------------------------------------
if (-not $SkipPrereqCheck) {
    Write-Step 0 "Prerequisite check"
    & "$PSScriptRoot\Check-Prerequisites.ps1" -Game $Game
    if ($LASTEXITCODE -ne 0) {
        throw "Prerequisite check failed. Run .\scripts\Install-Prerequisites.ps1 first."
    }
}

# ---------------------------------------------------------------------------
# 1. Git submodules
# ---------------------------------------------------------------------------
Write-Step 1 "Updating Git submodules"
Push-Location $RepoRoot
try {
    git submodule update --jobs=16 --init
    if ($LASTEXITCODE -ne 0) { throw "git submodule update failed" }
    Write-Ok "Submodules updated."
} finally {
    Pop-Location
}
Stop-IfRequested 'Submodules'

# ---------------------------------------------------------------------------
# 2. setuptools (required for Python 3.12+)
# ---------------------------------------------------------------------------
Write-Step 2 "Installing/Upgrading setuptools"
py -3 -m pip install --upgrade pip setuptools
if ($LASTEXITCODE -ne 0) { throw "pip install setuptools failed" }
Write-Ok "setuptools ready."
Stop-IfRequested 'Setuptools'

# ---------------------------------------------------------------------------
# 3. fxd get-chrome (downloads CEF)
# ---------------------------------------------------------------------------
Write-Step 3 "Downloading CEF via fxd get-chrome"
if (-not (Test-Path $Fxd)) { throw "fxd.ps1 not found at $Fxd" }
& $Fxd get-chrome
if ($LASTEXITCODE -ne 0) { throw "fxd get-chrome failed" }
Write-Ok "CEF downloaded."
Stop-IfRequested 'Chrome'

# ---------------------------------------------------------------------------
# 4. prebuild (natives)
# ---------------------------------------------------------------------------
Write-Step 4 "Running prebuild"
if (-not (Test-Path $Prebuild)) { throw "prebuild.cmd not found at $Prebuild" }

# prebuild.cmd is a cmd script that expects MSYS2 on PATH; run it via cmd.exe
$proc = Start-Process -FilePath "cmd.exe" -ArgumentList "/c `"$Prebuild`"" `
    -WorkingDirectory $RepoRoot -Wait -NoNewWindow -PassThru
if ($proc.ExitCode -ne 0) { throw "prebuild.cmd failed with exit code $($proc.ExitCode)" }
Write-Ok "Prebuild completed."
Stop-IfRequested 'Prebuild'

# ---------------------------------------------------------------------------
# 5. fxd gen -game <Game>
# ---------------------------------------------------------------------------
Write-Step 5 "Generating Visual Studio project files (fxd gen -game $Game)"
& $Fxd gen -game $Game
if ($LASTEXITCODE -ne 0) { throw "fxd gen failed" }
Write-Ok "Project files generated."
Stop-IfRequested 'Gen'

# ---------------------------------------------------------------------------
# 6. fxd vs  OR  fxd build
# ---------------------------------------------------------------------------
if ($BuildOnly) {
    Write-Step 6 "Building (fxd build -game $Game -configuration $Configuration)"
    & $Fxd build -game $Game -configuration $Configuration
    if ($LASTEXITCODE -ne 0) { throw "fxd build failed" }
    Write-Ok "Build completed."
} else {
    Write-Step 6 "Opening Visual Studio (fxd vs -game $Game)"
    & $Fxd vs -game $Game
    if ($LASTEXITCODE -ne 0) { throw "fxd vs failed" }
}

Write-Host "`n========================================" -ForegroundColor Green
Write-Host "VMP setup complete for game: $Game" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
