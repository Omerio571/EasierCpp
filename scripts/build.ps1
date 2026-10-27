param([switch]$Doctor)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path $vswhere)) { throw 'Install Visual Studio with Desktop development with C++.' }
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'No Visual Studio C++ toolchain found.' }
$devcmd = Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
$cmakeBin = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin'
$ninjaBin = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja'
# Import the developer environment into this script process.
$environment = & $env:ComSpec /d /c "call `"$devcmd`" -arch=x64 -host_arch=x64 >nul && set"
if ($LASTEXITCODE -ne 0) { throw 'Visual Studio environment setup failed.' }
foreach ($line in $environment) {
    if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
}
$env:PATH = "$cmakeBin;$ninjaBin;$env:PATH"
Push-Location $projectRoot
try {
    & cmake --preset windows-debug
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & cmake --build --preset windows-debug
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    & ctest --preset windows-debug
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
    if ($Doctor) {
        & ./build/debug/cpplivelab.exe doctor
        if ($LASTEXITCODE -ne 0) { throw 'Doctor failed.' }
    }
} finally { Pop-Location }
