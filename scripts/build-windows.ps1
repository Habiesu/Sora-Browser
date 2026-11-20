<#
.SYNOPSIS
  Compila Sora en Windows y genera el instalador en dist\.

.DESCRIPTION
  Requisitos (se comprueban al empezar): Visual Studio 2022 Build Tools con C++, CMake, Ninja,
  Rust (rustup, toolchain MSVC), Python 3, Inno Setup 6 y Git.
  Qt 6.12.0 (msvc2022_64 + qtwebengine) se descarga con scripts\fetch-qt.py en .\.qt si no existe.
#>
param(
    [string]$QtVersion = "6.12.0",
    [string]$QtDir = "",
    [string]$Version = "0.1.0",
    [switch]$SkipTests
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

function Require([string]$Cmd, [string]$Hint) {
    if (-not (Get-Command $Cmd -ErrorAction SilentlyContinue)) { throw "Falta $Cmd. $Hint" }
}

# Entorno de compilación de MSVC (x64)
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "Falta Visual Studio 2022 Build Tools con C++." }
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "Visual Studio no tiene las herramientas de C++ (VC.Tools.x86.x64)." }
# vcvars64.bat llama a vswhere.exe sin ruta; sin esto avisa de que no lo encuentra
$env:PATH = "$(Split-Path $vswhere);$env:PATH"
$vars = cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul && set"
foreach ($line in $vars) { if ($line -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($matches[1])" -Value $matches[2] } }

Require cmake "Instala CMake."
Require ninja "Instala Ninja."
Require cargo "Instala Rust con rustup."
Require git "Instala Git."
$iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
if (-not (Test-Path $iscc)) { $iscc = "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe" }
if (-not (Test-Path $iscc)) { throw "Falta Inno Setup 6." }

# Qt
if (-not $QtDir) { $QtDir = Join-Path $root ".qt\$QtVersion\msvc2022_64" }
if (-not (Test-Path "$QtDir\bin\Qt6WebEngineCore.dll")) {
    Require python "Instala Python 3."
    python -m pip install --user --quiet py7zr
    if ($LASTEXITCODE) { throw "No se pudo instalar py7zr." }
    python (Join-Path $PSScriptRoot "fetch-qt.py") $QtVersion $QtDir
    if ($LASTEXITCODE) { throw "No se pudo descargar Qt $QtVersion." }
}

# Compilar y probar
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$QtDir"
if ($LASTEXITCODE) { throw "Falló la configuración de CMake." }
cmake --build build-win
if ($LASTEXITCODE) { throw "Falló la compilación." }
if (-not $SkipTests) {
    $env:PATH = "$QtDir\bin;$env:PATH"
    ctest --test-dir build-win --output-on-failure
    if ($LASTEXITCODE) { throw "Fallaron las pruebas." }
}

# Reunir archivos y crear el instalador
$stage = Join-Path $root "dist\Sora"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
# qt_standard_project_setup() deja los .exe en la raíz del build en Windows
Copy-Item "build-win\sora.exe" $stage
& "$QtDir\bin\windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw --no-compiler-runtime "$stage\sora.exe"
if ($LASTEXITCODE) { throw "Falló windeployqt." }
# Runtime de C++ junto a sora.exe: así funciona en PCs sin "Visual C++ Redistributable"
# y sin pedir permisos de administrador.
$crt = Get-ChildItem -Directory "$env:VCToolsRedistDir\x64" -Filter "Microsoft.VC*.CRT" | Select-Object -First 1
if (-not $crt) { throw "No se encuentra el runtime de C++ en $env:VCToolsRedistDir." }
Copy-Item "$($crt.FullName)\*.dll" $stage

& $iscc /Qp "/DAppVersion=$Version" "/DSourceDir=$stage" "/DOutputDir=$(Join-Path $root 'dist')" "installer\sora.iss"
if ($LASTEXITCODE) { throw "Falló Inno Setup." }
Write-Host "Instalador listo en: $(Join-Path $root 'dist')" -ForegroundColor Green
