param(
    [switch]$Doctor,
    [string]$TestFilter,
    [ValidateSet('Quick', 'Full', 'Libraries')][string]$TestMode = 'Full',
    [ValidateRange(1, 16)][int]$Jobs = 1,
    [switch]$Timing,
    [string]$BuildDirectory
)
$ErrorActionPreference = 'Stop'
$phaseClock = [Diagnostics.Stopwatch]::StartNew()
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $projectRoot 'build/debug' }
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
[IO.Directory]::CreateDirectory($BuildDirectory) | Out-Null
# The lease spans configure/build/CTest. Cancellation closes it with the process.
$testLease = [IO.File]::Open((Join-Path $BuildDirectory 'cpplivelab-tests.lock'), 'OpenOrCreate', 'ReadWrite', 'None')
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
if ($Timing) { Write-Host ('[Timing] Manager SDK setup: {0:F3} s' -f $phaseClock.Elapsed.TotalSeconds) }
Push-Location $projectRoot
try {
    $phaseClock.Restart()
    & cmake --preset windows-debug -B $BuildDirectory
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    if ($Timing) { Write-Host ('[Timing] Manager CMake configuration: {0:F3} s' -f $phaseClock.Elapsed.TotalSeconds) }
    $phaseClock.Restart()
    & cmake --build $BuildDirectory
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    if ($Timing) { Write-Host ('[Timing] Manager build: {0:F3} s' -f $phaseClock.Elapsed.TotalSeconds) }
    $testArguments = @('--test-dir', $BuildDirectory, '--output-on-failure', '-L', ('^' + $TestMode.ToLowerInvariant() + '$'),
        '--parallel', "$Jobs", '--no-tests=error')
    if ($TestFilter) { $testArguments += @('-R', $TestFilter) }
    if ($TestFilter) {
        Write-Host "CppLiveLab tests: filtered $TestMode ($Jobs worker(s)); not a complete approval run."
    } else {
        Write-Host "CppLiveLab tests: $TestMode ($Jobs worker(s)); stage approval requires unfiltered Full."
    }
    $phaseClock.Restart()
    $env:CPPLIVELAB_TEST_CHILD = '1'
    & ctest @testArguments
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
    if ($Timing) { Write-Host ('[Timing] {0} tests: {1:F3} s' -f $TestMode, $phaseClock.Elapsed.TotalSeconds) }
    if ($Doctor) {
        & (Join-Path $BuildDirectory 'cpplivelab.exe') doctor
        if ($LASTEXITCODE -ne 0) { throw 'Doctor failed.' }
    }
} finally { Pop-Location; $testLease.Dispose() }
