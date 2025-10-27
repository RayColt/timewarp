<#
install-vcpkg-and-sdl.ps1
Automates: clone vcpkg, bootstrap, optional add to PATH, integrate with VS, install SDL2 packages.
Usage:
  - Run in an elevated PowerShell or Developer PowerShell.
  - Change $InstallDir if you want a different location.
#>

param(
    [string]$InstallDir = "C:\dev\vcpkg",
    [string[]]$Packages = @("sdl2","sdl2-image","sdl2-mixer","sdl2-ttf"),
    [string]$Triplet = "x64-windows",
    [switch]$AddToPath = $true,
    [switch]$SkipBootstrap = $false
)

function Write-Info($m){ Write-Host "[INFO] $m" -ForegroundColor Cyan }
function Write-Err($m){ Write-Host "[ERROR] $m" -ForegroundColor Red }

# 1. ensure Git exists
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Err "git not found in PATH. Install Git for Windows and retry."
    exit 1
}

# 2. create parent directory
if (-not (Test-Path $InstallDir)) {
    Write-Info "Creating $InstallDir"
    New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
}

# 3. clone or update vcpkg
if (-not (Test-Path (Join-Path $InstallDir ".git"))) {
    Write-Info "Cloning vcpkg into $InstallDir"
    git clone https://github.com/microsoft/vcpkg.git $InstallDir
    if ($LASTEXITCODE -ne 0) { Write-Err "git clone failed"; exit 1 }
} else {
    Write-Info "vcpkg already cloned, pulling latest"
    Push-Location $InstallDir
    git pull
    if ($LASTEXITCODE -ne 0) { Write-Err "git pull failed"; Pop-Location; exit 1 }
    Pop-Location
}

# 4. bootstrap vcpkg (build vcpkg.exe)
Push-Location $InstallDir
if (-not $SkipBootstrap) {
    if (-not (Test-Path ".\vcpkg.exe")) {
        Write-Info "Bootstrapping vcpkg (this may take a minute)..."
        & .\bootstrap-vcpkg.bat
        if ($LASTEXITCODE -ne 0) { Write-Err "bootstrap-vcpkg.bat failed"; Pop-Location; exit 1 }
    } else {
        Write-Info "vcpkg.exe already exists; skipping bootstrap."
    }
} else {
    Write-Info "Skipping bootstrap as requested."
}

# 5. (optional) add vcpkg to user PATH
if ($AddToPath) {
    $currentUserPath = [Environment]::GetEnvironmentVariable("PATH", "User")
    if ($currentUserPath -notlike "*$InstallDir*") {
        Write-Info "Adding $InstallDir to user PATH"
        $newPath = "$currentUserPath;$InstallDir"
        [Environment]::SetEnvironmentVariable("PATH", $newPath, "User")
        Write-Info "Added to user PATH. Restart shells to pick up the change."
    } else {
        Write-Info "vcpkg already in user PATH"
    }
}

# 6. integrate with Visual Studio
Write-Info "Running: vcpkg integrate install"
& .\vcpkg.exe integrate install
if ($LASTEXITCODE -ne 0) { Write-Err "vcpkg integrate failed"; Pop-Location; exit 1 }

# 7. install packages
$pkgList = $Packages -join " "
Write-Info "Installing packages: $pkgList for triplet $Triplet"
& .\vcpkg.exe install $Packages --triplet $Triplet
if ($LASTEXITCODE -ne 0) { Write-Err "vcpkg install failed"; Pop-Location; exit 1 }

Pop-Location

Write-Info "Done. Installed vcpkg and packages to $InstallDir"
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  - Restart Visual Studio (if open) so vcpkg integration is recognized."
Write-Host "  - For CMake: pass -DCMAKE_TOOLCHAIN_FILE=$InstallDir\scripts\buildsystems\vcpkg.cmake"
Write-Host "  - To change triplet, re-run script with -Triplet x86-windows"
