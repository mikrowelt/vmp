#Requires -Version 5.1
<#
.SYNOPSIS
    Audits the local Windows machine for everything VMP needs to build.
.DESCRIPTION
    Checks Visual Studio 2022 (with the exact workloads/components), PowerShell 7,
    Python 3.8+ with the `py` launcher, MSYS2 at C:\msys64, Node.js, Yarn, Git symlinks,
    and disk space. Reports PASS/FAIL for each item.
.PARAMETER Game
    Which game target to validate for (five, rdr3, ny, server). Default: five.
.PARAMETER MinDiskGB
    Minimum free disk space (in GB) to report as OK. Default: 120.
.EXAMPLE
    .\Check-Prerequisites.ps1
    .\Check-Prerequisites.ps1 -Game server
#>
[CmdletBinding()]
param(
    [ValidateSet('five', 'rdr3', 'ny', 'server')]
    [string]$Game = 'five',
    [int]$MinDiskGB = 120
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# Colours & helpers
# ---------------------------------------------------------------------------
function Write-Ok($msg) { Write-Host "[PASS] $msg" -ForegroundColor Green }
function Write-Fail($msg) { Write-Host "[FAIL] $msg" -ForegroundColor Red }
function Write-Warn($msg) { Write-Host "[WARN] $msg" -ForegroundColor Yellow }
function Write-Info($msg) { Write-Host "[INFO] $msg" -ForegroundColor Cyan }

$script:AllPassed = $true
function Register-Fail { $script:AllPassed = $false }

# ---------------------------------------------------------------------------
# 1. PowerShell version
# ---------------------------------------------------------------------------
Write-Info "Checking PowerShell version..."
if ($PSVersionTable.PSVersion.Major -ge 7) {
    Write-Ok "PowerShell $($PSVersionTable.PSVersion)"
} else {
    Write-Fail "PowerShell $($PSVersionTable.PSVersion). VMP requires PowerShell 7+."
    Write-Info "Install: winget install Microsoft.PowerShell"
    Register-Fail
}

# ---------------------------------------------------------------------------
# 2. Disk space on the repo drive
# ---------------------------------------------------------------------------
$RepoRoot = (Resolve-Path "$PSScriptRoot\..").Path
$Drive = (Get-Item $RepoRoot).PSDrive.Name
$FreeGB = [math]::Round((Get-PSDrive $Drive).Free / 1GB, 2)
Write-Info "Checking free disk space on ${Drive}: ($FreeGB GB free)..."
if ($FreeGB -ge $MinDiskGB) {
    Write-Ok "${FreeGB} GB free (>= ${MinDiskGB} GB requested)"
} else {
    Write-Fail "${FreeGB} GB free. You should have at least ${MinDiskGB} GB for a full VMP build."
    Register-Fail
}

# ---------------------------------------------------------------------------
# 3. Git + symlinks
# ---------------------------------------------------------------------------
Write-Info "Checking Git and symlink support..."
$git = Get-Command git -ErrorAction SilentlyContinue
if (-not $git) {
    Write-Fail "Git is not in PATH."
    Write-Info "Install: winget install Git.Git"
    Register-Fail
} else {
    $gitVer = (git --version 2>$null) -replace 'git version ',''
    Write-Ok "Git $gitVer"
}

try {
    $symlinks = git config --get core.symlinks 2>$null
    if ($symlinks -eq 'true') {
        Write-Ok "Git core.symlinks=true"
    } else {
        Write-Fail "Git core.symlinks is '$symlinks' (should be 'true'). Run: git config --global core.symlinks true"
        Register-Fail
    }
} catch {
    Write-Fail "Could not read git config core.symlinks: $_"
    Register-Fail
}

# ---------------------------------------------------------------------------
# 4. Visual Studio 2022 + required workloads/components
# ---------------------------------------------------------------------------
Write-Info "Checking Visual Studio 2022 and required workloads/components..."
$Vswhere = "$PSScriptRoot\..\code\tools\ci\vswhere.exe"
if (-not (Test-Path $Vswhere)) {
    Write-Fail "Bundled vswhere.exe not found at $Vswhere. Repository may be incomplete."
    Register-Fail
} else {
    $VSPath = & $Vswhere -prerelease -latest -property installationPath -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 2>$null

    if ([string]::IsNullOrWhiteSpace($VSPath)) {
        Write-Fail "Visual Studio 2022 with C++ tools not found."
        Write-Info "Install VS 2022 Community with these workloads/components:"
        Write-Info "  Workloads: .NET desktop development, Desktop development with C++, Windows application development"
        Write-Info "  Components: .NET Framework 4.6 targeting pack, Windows 11 SDK (10.0.22000.0)"
        Register-Fail
    } else {
        $VSVer = & $Vswhere -prerelease -latest -property catalog_buildVersion -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 2>$null
        Write-Ok "Visual Studio found at $VSPath (build $VSVer)"

        # Check workloads/components using vswhere's requires syntax
        $Required = @(
            @{ Id = 'Microsoft.VisualStudio.Workload.NetDesktop'; Name = '.NET desktop development workload' },
            @{ Id = 'Microsoft.VisualStudio.Workload.NativeDesktop'; Name = 'Desktop development with C++ workload' },
            @{ Id = 'Microsoft.VisualStudio.Workload.Universal'; Name = 'Windows application development workload' },
            @{ Id = 'Microsoft.VisualStudio.Component.NetFx46.TargetingPack'; Name = '.NET Framework 4.6 targeting pack' },
            @{ Id = 'Microsoft.VisualStudio.Component.Windows11SDK.22000'; Name = 'Windows 11 SDK (10.0.22000.0)' }
        )

        foreach ($item in $Required) {
            $found = & $Vswhere -prerelease -latest -products * -requires $item.Id -property installationPath 2>$null
            if ($found) {
                Write-Ok "$($item.Name)"
            } else {
                Write-Fail "$($item.Name) is missing."
                Register-Fail
            }
        }
    }
}

# ---------------------------------------------------------------------------
# 5. Python + py launcher + setuptools
# ---------------------------------------------------------------------------
Write-Info "Checking Python..."
$py = Get-Command py -ErrorAction SilentlyContinue
if (-not $py) {
    Write-Fail "Python 'py' launcher not found. VMP expects `py` to be on PATH."
    Write-Info "Install Python 3.8+ from python.org and tick 'Add to PATH' + 'py launcher'."
    Register-Fail
} else {
    try {
        $pyVerStr = py --version 2>&1
        $pyVer = $pyVerStr -replace 'Python ',''
        Write-Ok "Python launcher found: $pyVer"
    } catch {
        Write-Fail "Could not run `py --version`: $_"
        Register-Fail
    }
}

try {
    $pythonVer = py -3 --version 2>&1
    if ($pythonVer -match 'Python (\d+)\.(\d+)') {
        $maj = [int]$Matches[1]; $min = [int]$Matches[2]
        if (($maj -gt 3) -or ($maj -eq 3 -and $min -ge 8)) {
            Write-Ok "Default Python 3 is $maj.$min (>= 3.8)"
        } else {
            Write-Fail "Default Python 3 is $maj.$min. Need 3.8+."
            Register-Fail
        }
    } else {
        Write-Fail "Could not parse Python version: $pythonVer"
        Register-Fail
    }
} catch {
    Write-Fail "Could not determine Python 3 version: $_"
    Register-Fail
}

try {
    $hasSetuptools = py -3 -c "import setuptools" 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Ok "setuptools is installed"
    } else {
        Write-Fail "setuptools is NOT installed. Run: py -3 -m pip install setuptools"
        Register-Fail
    }
} catch {
    Write-Fail "Could not check setuptools: $_"
    Register-Fail
}

# ---------------------------------------------------------------------------
# 6. MSYS2
# ---------------------------------------------------------------------------
Write-Info "Checking MSYS2 at C:\msys64..."
$Msys2Path = 'C:\msys64'
if (-not (Test-Path $Msys2Path)) {
    Write-Fail "C:\msys64 not found. MSYS2 must be installed at that exact path."
    Write-Info "Install: winget install MSYS2.MSYS2"
    Register-Fail
} else {
    Write-Ok "MSYS2 directory exists at $Msys2Path"
    $pacman = Join-Path $Msys2Path 'usr\bin\pacman.exe'
    if (Test-Path $pacman) {
        Write-Ok "pacman.exe found"
    } else {
        Write-Fail "pacman.exe not found inside C:\msys64\usr\bin. MSYS2 install is incomplete."
        Register-Fail
    }
}

# ---------------------------------------------------------------------------
# 7. Node.js
# ---------------------------------------------------------------------------
Write-Info "Checking Node.js..."
$node = Get-Command node -ErrorAction SilentlyContinue
if (-not $node) {
    Write-Fail "Node.js is not in PATH."
    Write-Info "Install: winget install OpenJS.NodeJS.LTS"
    Register-Fail
} else {
    $nodeVer = node --version 2>$null
    Write-Ok "Node.js $nodeVer"
}

# ---------------------------------------------------------------------------
# 8. Yarn
# ---------------------------------------------------------------------------
Write-Info "Checking Yarn..."
$yarn = Get-Command yarn -ErrorAction SilentlyContinue
if (-not $yarn) {
    Write-Fail "Yarn is not in PATH."
    Write-Info "Install: corepack enable  (or)  winget install Yarn.Yarn  (or)  npm install -g yarn"
    Register-Fail
} else {
    $yarnVer = yarn --version 2>$null
    Write-Ok "Yarn $yarnVer"
}

# ---------------------------------------------------------------------------
# 9. Repo submodules (warn only)
# ---------------------------------------------------------------------------
Write-Info "Checking Git submodules..."
Push-Location "$PSScriptRoot\.."
try {
    $smStatus = git submodule status 2>$null
    $missing = $smStatus | Where-Object { $_ -match '^-' }
    if ($missing) {
        Write-Warn "Some submodules are not initialised. Run: git submodule update --jobs=16 --init"
    } else {
        Write-Ok "Submodules appear to be initialised"
    }
} finally {
    Pop-Location
}

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
Write-Host "`n========================================" -ForegroundColor Cyan
if ($script:AllPassed) {
    Write-Host "ALL CHECKS PASSED" -ForegroundColor Green
    Write-Host "You can now run: .\scripts\Setup-VMP.ps1 -Game $Game" -ForegroundColor Green
} else {
    Write-Host "SOME CHECKS FAILED" -ForegroundColor Red
    Write-Host "Run: .\scripts\Install-Prerequisites.ps1 -Game $Game" -ForegroundColor Yellow
    exit 1
}
Write-Host "========================================" -ForegroundColor Cyan
