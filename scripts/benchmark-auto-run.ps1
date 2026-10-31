param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$ReportDirectory,
    [switch]$ProfileBuild,
    [ValidateRange(3,30)][int]$Samples = 3
)
$ErrorActionPreference = 'Stop'
$Executable = [IO.Path]::GetFullPath($Executable)
$ReportDirectory = [IO.Path]::GetFullPath($ReportDirectory)
[IO.Directory]::CreateDirectory($ReportDirectory) | Out-Null
$fixture = Join-Path $env:TEMP ('EasierCppBenchmark-' + [Guid]::NewGuid().ToString('N'))
if (Test-Path -LiteralPath $fixture) { throw 'Benchmark fixture already exists.' }
[IO.Directory]::CreateDirectory($fixture) | Out-Null
$oldState = $env:CPPLIVELAB_STATE_HOME
$oldProfile = $env:EASIERCPP_PROFILE_BUILD
$profiles = [Collections.Generic.List[object]]::new()
$rows = [Collections.Generic.List[object]]::new()
function Measure-CommandLine([string]$Name, [string]$Program, [string[]]$Arguments) {
    $log = Join-Path $ReportDirectory ($Name + '.log')
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $parentTick = if ($ProfileBuild) { [EasierCppBenchmarkClock]::GetTickCount64() } else { 0 }
    # Windows PowerShell 5 turns redirected native stderr into ErrorRecords.
    # Retain warning/error text, but use the native exit code to decide success.
    $previousErrorAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $Program @Arguments *> $log
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorAction
    }
    $elapsed = $watch.Elapsed.TotalMilliseconds
    $rows.Add([pscustomobject]@{Case=$Name; Milliseconds=[Math]::Round($elapsed,2); ExitCode=$code})
    if ($ProfileBuild) {
        $profile = [ordered]@{Case=$Name; TotalMs=[Math]::Round($elapsed,2)}
        foreach ($line in [IO.File]::ReadAllLines($log)) {
            if ($line -match '^EASIERCPP_PROFILE ([a-z_]+)=([0-9.]+)$') {
                $profile[$matches[1]] = [double]::Parse($matches[2], [Globalization.CultureInfo]::InvariantCulture)
            }
        }
        if ($profile.Contains('worker_start_tick')) {
            $profile['worker_startup_ms'] = $profile['worker_start_tick'] - $parentTick
            $profile['cli_dispatch_ms'] = $profile['managed_entry_tick'] - $profile['worker_start_tick']
            $profiles.Add([pscustomobject]$profile)
        }
    }
    if ($code -ne 0 -and $Name -ne 'failure') { throw "Benchmark failed: $Name. See $log" }
    return $code
}
try {
    if ($ProfileBuild) {
        $env:EASIERCPP_PROFILE_BUILD = '1'
        if (-not ('EasierCppBenchmarkClock' -as [type])) {
            Add-Type 'public static class EasierCppBenchmarkClock { [System.Runtime.InteropServices.DllImport("kernel32.dll")] public static extern ulong GetTickCount64(); }'
        }
    }
    $env:CPPLIVELAB_STATE_HOME = Join-Path $fixture 'state'
    $source = Join-Path $fixture 'source'
    [IO.Directory]::CreateDirectory($source) | Out-Null
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw 'Visual Studio C++ tools were not discovered.' }
    $setup = [IO.Path]::GetFullPath((Join-Path $vs 'Common7/Tools/VsDevCmd.bat'))
    $watch = [Diagnostics.Stopwatch]::StartNew()
    # Capture privately into this disposable PowerShell process, never into a log.
    $prepared = & $env:ComSpec /d /c "call `"$setup`" -arch=x64 -host_arch=x64 >nul && set"
    if ($LASTEXITCODE -ne 0) { throw 'SDK setup failed.' }
    foreach ($line in $prepared) {
        if ($line -match '^([^=]+)=(.*)$') {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
        }
    }
    $env:CPPLIVELAB_PREPARED_VS = $setup
    $rows.Add([pscustomobject]@{Case='sdk_prepare_once'; Milliseconds=[Math]::Round($watch.Elapsed.TotalMilliseconds,2); ExitCode=0})
    $cmake = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
    $ninja = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
    $compiler = Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    [IO.File]::WriteAllText((Join-Path $source 'CMakeLists.txt'), @'
cmake_minimum_required(VERSION 3.24)
project(AutoRunBenchmark LANGUAGES CXX)
add_library(dependency STATIC dependency.cpp)
add_executable(app main.cpp)
target_compile_features(dependency PUBLIC cxx_std_20)
target_link_libraries(app PRIVATE dependency)
'@)
    [IO.File]::WriteAllText((Join-Path $source 'shared.hpp'), "#pragma once`n#define VALUE 42`nint value();`n")
    [IO.File]::WriteAllText((Join-Path $source 'dependency.cpp'), "#include `"shared.hpp`"`nint value()`n{`n    return VALUE;`n}`n")
    $main = @'
#include "shared.hpp"
#include <iostream>
#include <string>
int main()
{
    std::string name;
    int a = 0, b = 0;
    std::cout << "Name and two numbers: " << std::flush;
    if (!(std::cin >> name >> a >> b))
    {
        return 1;
    }
    std::cout << name << " sum " << a + b << " library " << value() << '\n';
    return 0;
}
'@
    [IO.File]::WriteAllText((Join-Path $source 'main.cpp'), $main)
    $toolBuild = Join-Path $fixture 'tool-build'
    Measure-CommandLine 'cmake_configure_cold' $cmake @('-S',$source,'-B',$toolBuild,'-G','Ninja','-DCMAKE_BUILD_TYPE=Debug',"-DCMAKE_MAKE_PROGRAM=$ninja","-DCMAKE_CXX_COMPILER=$compiler") | Out-Null
    Measure-CommandLine 'compile_link_cold' $cmake @('--build',$toolBuild) | Out-Null
    Measure-CommandLine 'ninja_noop' $cmake @('--build',$toolBuild) | Out-Null
    foreach ($slot in @('fresh-auto-a','fresh-auto-b')) {
        Measure-CommandLine "cold_$slot" $Executable @('manage-build',$source,'--target','app','--build-slot',$slot) | Out-Null
    }
    $artifact = Get-ChildItem -LiteralPath $env:CPPLIVELAB_STATE_HOME -Recurse -Filter app.exe | Where-Object FullName -match 'fresh-auto-a' | Select-Object -First 1
    if (-not $artifact) { throw 'Selected managed artifact was not discovered.' }
    $dependency = Join-Path $artifact.DirectoryName 'dependency.lib'
    $written = $artifact.LastWriteTimeUtc
    $libraryWritten = [IO.File]::GetLastWriteTimeUtc($dependency)
    Measure-CommandLine 'warm_noop' $Executable @('manage-build',$source,'--target','app','--build-slot','fresh-auto-a','--incremental') | Out-Null
    if ([IO.File]::GetLastWriteTimeUtc($artifact.FullName) -ne $written) { throw 'No-op rebuilt the executable.' }
    for ($sample=0; $sample -lt $Samples; $sample++) {
        [IO.File]::WriteAllText((Join-Path $source 'main.cpp'), $main + "`n// source sample $sample`n")
        Measure-CommandLine "source_$sample" $Executable @('manage-build',$source,'--target','app','--build-slot','fresh-auto-a','--incremental') | Out-Null
        if ([IO.File]::GetLastWriteTimeUtc($dependency) -ne $libraryWritten) { throw 'Unchanged dependency was rebuilt.' }
    }
    [IO.File]::WriteAllText((Join-Path $source 'shared.hpp'), "#pragma once`n#define VALUE 43`nint value();`n")
    Measure-CommandLine 'header' $Executable @('manage-build',$source,'--target','app','--build-slot','fresh-auto-a','--incremental') | Out-Null
    if ([IO.File]::GetLastWriteTimeUtc($dependency) -eq $libraryWritten) { throw 'Header dependency was not rebuilt.' }
    [IO.File]::WriteAllText((Join-Path $source 'main.cpp'), 'intentional compilation failure')
    if ((Measure-CommandLine 'failure' $Executable @('manage-build',$source,'--target','app','--build-slot','fresh-auto-a','--incremental')) -eq 0) { throw 'Invalid source unexpectedly compiled.' }
    [IO.File]::WriteAllText((Join-Path $source 'main.cpp'), $main)
    Measure-CommandLine 'recovery' $Executable @('manage-build',$source,'--target','app','--build-slot','fresh-auto-a','--incremental') | Out-Null
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $output = 'Ada 2 3' | & $artifact.FullName
    if ($LASTEXITCODE -ne 0 -or ($output -join "`n") -notmatch 'Ada sum 5 library 43') { throw 'Interactive console benchmark failed.' }
    $rows.Add([pscustomobject]@{Case='console_with_input'; Milliseconds=[Math]::Round($watch.Elapsed.TotalMilliseconds,2); ExitCode=0})
    $rows | Export-Csv -LiteralPath (Join-Path $ReportDirectory 'timings.csv') -NoTypeInformation
    $rows | Format-Table
    if ($ProfileBuild) {
        # A failure stops before artifact verification; union the successful row's columns.
        $columns = $profiles | ForEach-Object { $_.PSObject.Properties.Name } | Select-Object -Unique
        $profiles | Select-Object $columns | Export-Csv -LiteralPath (Join-Path $ReportDirectory 'profiles.csv') -NoTypeInformation
    }
} finally {
    $env:CPPLIVELAB_STATE_HOME = $oldState
    $env:EASIERCPP_PROFILE_BUILD = $oldProfile
    $resolved = [IO.Path]::GetFullPath($fixture)
    if ([IO.Path]::GetDirectoryName($resolved) -ne [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') -or [IO.Path]::GetFileName($resolved) -notlike 'EasierCppBenchmark-*') { throw 'Unsafe benchmark cleanup path.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
