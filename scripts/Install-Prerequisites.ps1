#Requires -Version 7.0
<#
.SYNOPSIS
    Installs missing VMP build prerequisites using winget where possible.
.DESCRIPTION
    Runs a prerequisite audit and then installs the missing pieces:
      * PowerShell 7
      * Git (if missing)
      * Visual Studio 2022 Community with the required workloads/components
      * Python 3.11 + py launcher
      * MSYS2 to C:\msys64
      * Node.js LTS
      * Yarn
    The script is interactive by default and asks before large installs.
.PARAMETER AutoInstall
    Skip confirmation prompts and install everything automatically.
.PARAMETER Game
    Not used during install, kept for consistency with the other scripts.
.EXAMPLE
    .\Install-Prerequisites.ps1
    .\Install-Prerequisites.ps1 -AutoInstall
#>
[CmdletBinding()]
param(
    [switch]$AutoInstall,
    [ValidateSet('five', 'rdr3', 'ny', 'server')]
    [string]$Game = 'five'
)

$ErrorActionPreference = 'Stop'

function Write-Info($msg) { Write-Host "[INFO] $msg" -ForegroundColor Cyan }
function Write-Ok($msg) { Write-Host "[OK]   $msg" -ForegroundColor Green }
function Write-Warn($msg) { Write-Host "[WARN] $msg" -ForegroundColor Yellow }

# ---------------------------------------------------------------------------
# Sanity checks
# ---------------------------------------------------------------------------
if (-not $IsWindows) {
    throw "This install script is Windows-only."
}

$winget = Get-Command winget -ErrorAction SilentlyContinue
if (-not $winget) {
    throw "winget was not found. Install App Installer / Windows Package Manager first, then re-run."
}

# Run as admin is required for most installers
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
$isAdmin = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Warn "This script should be run as Administrator for installations to succeed."
    if (-not $AutoInstall) {
        $continue = Read-Host "Continue anyway? (y/N)"
        if ($continue -notmatch '^[Yy]') { exit }
    }
}

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
function Invoke-IfMissing {
    param(
        [string]$Name,
        [scriptblock]$Test,
        [scriptblock]$Install
    )
    Write-Info "Checking $Name..."
    $found = & $Test
    if ($found) {
        Write-Ok "$Name already installed/satisfied."
        return
    }
    Write-Warn "$Name is missing."
    if (-not $AutoInstall) {
        $answer = Read-Host "Install $Name now? (Y/n)"
        if ($answer -match '^[Nn]') {
            Write-Warn "Skipping $Name. The build may fail without it."
            return
        }
    }
    Write-Info "Installing $Name..."
    & $Install
}

function Test-VisualStudioInstalled {
    $Vswhere = "$PSScriptRoot\..\code\tools\ci\vswhere.exe"
    if (-not (Test-Path $Vswhere)) { return $false }
    $path = & $Vswhere -prerelease -latest -property installationPath -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 2>$null
    return -not [string]::IsNullOrWhiteSpace($path)
}

function Test-VSWorkload {
    param([string]$Id)
    $Vswhere = "$PSScriptRoot\..\code\tools\ci\vswhere.exe"
    if (-not (Test-Path $Vswhere)) { return $false }
    $path = & $Vswhere -prerelease -latest -products * -requires $Id -property installationPath 2>$null
    return -not [string]::IsNullOrWhiteSpace($path)
}

# ---------------------------------------------------------------------------
# 1. PowerShell 7
# ---------------------------------------------------------------------------
Invoke-IfMissing -Name 'PowerShell 7+' -Test {
    $PSVersionTable.PSVersion.Major -ge 7
} -Install {
    winget install --id Microsoft.PowerShell --source winget --accept-source-agreements --accept-package-agreements
}

# ---------------------------------------------------------------------------
# 2. Git
# ---------------------------------------------------------------------------
Invoke-IfMissing -Name 'Git' -Test {
    [bool](Get-Command git -ErrorAction SilentlyContinue)
} -Install {
    winget install --id Git.Git --source winget --accept-source-agreements --accept-package-agreements
    Write-Warn "Git was installed. Restart your terminal/PowerShell so `git` is in PATH."
}

# ---------------------------------------------------------------------------
# 3. Visual Studio 2022 Community + workloads/components
# ---------------------------------------------------------------------------
$VsOk = Test-VisualStudioInstalled
$VsWorkloadIds = @(
    'Microsoft.VisualStudio.Workload.NetDesktop'
    'Microsoft.VisualStudio.Workload.NativeDesktop'
    'Microsoft.VisualStudio.Workload.Universal'
    'Microsoft.VisualStudio.Component.NetFx46.TargetingPack'
    'Microsoft.VisualStudio.Component.Windows11SDK.22000'
)
$MissingWorkloads = $VsWorkloadIds | Where-Object { -not (Test-VSWorkload -Id $_) }

if ($VsOk -and -not $MissingWorkloads) {
    Write-Ok "Visual Studio 2022 with all required workloads/components is already installed."
} else {
    if (-not $VsOk) {
        Write-Warn "Visual Studio 2022 (Community or higher) is missing or lacks C++ tools."
    } else {
        Write-Warn "Visual Studio 2022 is installed but missing components: $($MissingWorkloads -join ', ')"
    }

    if (-not $AutoInstall) {
        $answer = Read-Host "Install/Modify Visual Studio 2022 Community with the required workloads? This is large (several GB). (Y/n)"
        if ($answer -match '^[Nn]') { continue }
    }

    # Build a single --override string that adds all required items
    $addArgs = $VsWorkloadIds | ForEach-Object { "--add $_" }
    $override = "--passive --norestart --wait --includeRecommended --addProductLang en-US $($addArgs -join ' ')"

    Write-Info "Launching Visual Studio 2022 Community installer..."
    winget install --id Microsoft.VisualStudio.2022.Community `
        --source winget `
        --accept-source-agreements --accept-package-agreements `
        --override "$override"

    if ($LASTEXITCODE -eq 0) {
        Write-Ok "Visual Studio 2022 Community installed/modified successfully."
    } else {
        Write-Warn "Visual Studio installer returned $LASTEXITCODE. You may need to finish setup manually."
    }
}

# ---------------------------------------------------------------------------
# 4. Python 3.11 + py launcher
# ---------------------------------------------------------------------------
Invoke-IfMissing -Name 'Python (py launcher)' -Test {
    [bool](Get-Command py -ErrorAction SilentlyContinue)
} -Install {
    winget install --id Python.Python.3.11 --source winget --accept-source-agreements --accept-package-agreements
    Write-Warn "Python was installed. Restart your terminal/PowerShell so `py` is in PATH."
}

# ---------------------------------------------------------------------------
# 5. setuptools (if Python exists)
# ---------------------------------------------------------------------------
if (Get-Command py -ErrorAction SilentlyContinue) {
    $hasSetuptools = py -3 -c "import setuptools" 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Info "Installing setuptools..."
        py -3 -m pip install --upgrade pip setuptools
    } else {
        Write-Ok "setuptools already installed."
    }
}

# ---------------------------------------------------------------------------
# 6. MSYS2 to C:\msys64
# ---------------------------------------------------------------------------
Invoke-IfMissing -Name 'MSYS2 at C:\msys64' -Test {
    Test-Path 'C:\msys64\usr\bin\pacman.exe'
} -Install {
    winget install --id MSYS2.MSYS2 --source winget --accept-source-agreements --accept-package-agreements
    Write-Warn "MSYS2 was installed. If it is not at C:\msys64, move/reinstall it there; VMP hard-codes that path."
}

# ---------------------------------------------------------------------------
# 7. Node.js LTS
# ---------------------------------------------------------------------------
Invoke-IfMissing -Name 'Node.js LTS' -Test {
    [bool](Get-Command node -ErrorAction SilentlyContinue)
} -Install {
    winget install --id OpenJS.NodeJS.LTS --source winget --accept-source-agreements --accept-package-agreements
}

# ---------------------------------------------------------------------------
# 8. Yarn
# ---------------------------------------------------------------------------
Invoke-IfMissing -Name 'Yarn' -Test {
    [bool](Get-Command yarn -ErrorAction SilentlyContinue)
} -Install {
    # Prefer corepack because it is bundled with recent Node LTS
    if (Get-Command corepack -ErrorAction SilentlyContinue) {
        corepack enable
    } else {
        winget install --id Yarn.Yarn --source winget --accept-source-agreements --accept-package-agreements
    }
}

# ---------------------------------------------------------------------------
# 9. Git symlinks
# ---------------------------------------------------------------------------
Write-Info "Ensuring git core.symlinks=true..."
$current = git config --get core.symlinks 2>$null
if ($current -ne 'true') {
    git config --global core.symlinks true
    Write-Ok "Set git core.symlinks=true globally."
} else {
    Write-Ok "git core.symlinks=true already set."
}

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "Install pass complete." -ForegroundColor Green
Write-Host "Next, restart your terminal, then run:" -ForegroundColor Green
Write-Host "  .\scripts\Check-Prerequisites.ps1" -ForegroundColor Yellow
Write-Host "  .\scripts\Setup-VMP.ps1 -Game $Game" -ForegroundColor Yellow
Write-Host "========================================" -ForegroundColor Cyan
