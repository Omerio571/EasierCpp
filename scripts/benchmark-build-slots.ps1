param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$ReportDirectory,
    [ValidateRange(4,20)][int]$Samples = 8
)
$ErrorActionPreference = 'Stop'
$Executable = [IO.Path]::GetFullPath($Executable)
$ReportDirectory = [IO.Path]::GetFullPath($ReportDirectory)
[IO.Directory]::CreateDirectory($ReportDirectory) | Out-Null
$fixture = Join-Path $env:TEMP ('cll-slots-' + [Guid]::NewGuid().ToString('N'))
if (Test-Path -LiteralPath $fixture) { throw 'Fixture already exists.' }
[IO.Directory]::CreateDirectory($fixture) | Out-Null
$oldState = $env:CPPLIVELAB_STATE_HOME
$oldProfile = $env:EASIERCPP_PROFILE_BUILD
$rows = [Collections.Generic.List[object]]::new()
$process = $null
function Build-Slot([string]$Source, [string]$Slot, [string]$Log) {
    $previous = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $Executable manage-build $Source --target app --build-slot $Slot --incremental *> $Log
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previous }
    if ($code -ne 0) { throw "Slot experiment build failed: $Log" }
}
function Stop-Owned {
    if ($process -and -not $process.HasExited) {
        $process.StandardInput.WriteLine('stop')
        if (-not $process.WaitForExit(5000)) { $process.Kill(); $process.WaitForExit(); throw 'Prototype program did not shut down gracefully.' }
    }
}
try {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $setup = Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
    $prepared = & $env:ComSpec /d /c "call `"$setup`" -arch=x64 -host_arch=x64 >nul && set"
    if ($LASTEXITCODE -ne 0) { throw 'SDK preparation failed.' }
    foreach ($line in $prepared) {
        if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process') }
    }
    $env:CPPLIVELAB_PREPARED_VS = $setup
    $env:EASIERCPP_PROFILE_BUILD = '1'
    $env:CPPLIVELAB_STATE_HOME = Join-Path $fixture 'state'
    foreach ($architecture in @('alternating','shared-staged')) {
        $source = Join-Path $fixture $architecture
        [IO.Directory]::CreateDirectory($source) | Out-Null
        [IO.File]::WriteAllText((Join-Path $source 'CMakeLists.txt'), @'
cmake_minimum_required(VERSION 3.24)
project(SlotExperiment LANGUAGES CXX)
add_library(helper SHARED helper.cpp)
add_executable(app main.cpp first.cpp second.cpp)
target_compile_features(app PRIVATE cxx_std_20)
target_link_libraries(app PRIVATE helper)
add_custom_command(TARGET app POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/asset.txt" "$<TARGET_FILE_DIR:app>/asset.txt"
)
'@)
        [IO.File]::WriteAllText((Join-Path $source 'helper.cpp'), "extern `"C`" __declspec(dllexport) int helper()`n{`n    return 7;`n}`n")
        [IO.File]::WriteAllText((Join-Path $source 'asset.txt'), 'owned-asset')
        [IO.File]::WriteAllText((Join-Path $source 'main.cpp'), @'
#include <fstream>
#include <iostream>
#include <string>
extern "C" __declspec(dllimport) int helper();
int first();
int second();
int main(int argc, char** argv)
{
    std::string asset;
    std::ifstream("asset.txt") >> asset;
    if (asset != "owned-asset" || helper() != 7 || argc != 2 || std::string(argv[1]) != "owned-argument")
    {
        return 9;
    }
    std::cout << first() + second() << std::endl;
    std::string stop;
    std::getline(std::cin, stop);
    return 0;
}
'@)
        foreach ($name in @('first','second')) {
            [IO.File]::WriteAllText((Join-Path $source "$name.cpp"), "#include <iostream>`n#include <vector>`nint $name()`n{`n    return 0;`n}`n")
        }
        $slots = @(if ($architecture -eq 'alternating') { 'fresh-experiment-a'; 'fresh-experiment-b' } else { 'fresh-experiment-shared' })
        foreach ($slot in $slots) { Build-Slot $source $slot (Join-Path $ReportDirectory "$architecture-cold-$slot.log") }
        for ($sample=0; $sample -lt $Samples; $sample++) {
            $name = if ($sample % 2) { 'second' } else { 'first' }
            [IO.File]::WriteAllText((Join-Path $source "$name.cpp"), "#include <iostream>`n#include <vector>`nint $name()`n{`n    return $($sample+1);`n}`n")
            $slot = $slots[$sample % $slots.Count]
            $log = Join-Path $ReportDirectory "$architecture-$sample.log"
            $watch = [Diagnostics.Stopwatch]::StartNew()
            Build-Slot $source $slot $log
            $buildMs = $watch.Elapsed.TotalMilliseconds
            if ($process -and $process.HasExited) { throw 'Previous staged process was lost during compilation.' }
            $artifact = Get-ChildItem -LiteralPath $env:CPPLIVELAB_STATE_HOME -Recurse -Filter app.exe | Where-Object { $_.DirectoryName.EndsWith("-$slot") } | Select-Object -First 1
            if (-not $artifact) { throw 'Prototype artifact missing.' }
            $stage = Join-Path $fixture "runtime-$architecture-$sample"
            [IO.Directory]::CreateDirectory($stage) | Out-Null
            $stageWatch = [Diagnostics.Stopwatch]::StartNew()
            foreach ($file in Get-ChildItem -LiteralPath $artifact.DirectoryName -File | Where-Object { $_.Extension -in @('.exe','.dll','.pdb','.txt') }) {
                if ($file.Name -eq 'CMakeCache.txt') { continue }
                Copy-Item -LiteralPath $file.FullName -Destination $stage
            }
            $stageMs = $stageWatch.Elapsed.TotalMilliseconds
            Stop-Owned
            if ($process -and $process.ExitCode -ne 0) { throw 'Old staged consumer failed.' }
            if ($process) { $process.Dispose() }
            $start = [Diagnostics.ProcessStartInfo]::new((Join-Path $stage 'app.exe'), 'owned-argument')
            $start.WorkingDirectory = $stage
            $start.UseShellExecute = $false
            $start.CreateNoWindow = $true
            $start.RedirectStandardInput = $true
            $start.RedirectStandardOutput = $true
            $process = [Diagnostics.Process]::Start($start)
            $output = $process.StandardOutput.ReadLineAsync()
            if (-not $output.Wait(10000)) { throw 'Staged application did not become ready.' }
            $expected = if ($sample -eq 0) { 1 } else { 2*$sample+1 }
            if ($output.Result -ne "$expected") { throw 'Stale staged version, missing DLL, argument or asset.' }
            $compiled = @([IO.File]::ReadAllLines($log) | Where-Object { $_ -match 'Building CXX object' }).Count
            $rows.Add([pscustomobject]@{Architecture=$architecture;Sample=$sample;BuildMs=$buildMs;StageMs=$stageMs;ReadyMs=$watch.Elapsed.TotalMilliseconds;CompiledFiles=$compiled})
        }
        Stop-Owned
        if ($process.ExitCode -ne 0) { throw 'Final staged consumer failed.' }
        $process.Dispose()
        $process = $null
    }
    $rows | Export-Csv (Join-Path $ReportDirectory 'samples.csv') -NoTypeInformation
    $rows | Format-Table
} finally {
    Stop-Owned
    if ($process) { $process.Dispose() }
    $env:CPPLIVELAB_STATE_HOME = $oldState
    $env:EASIERCPP_PROFILE_BUILD = $oldProfile
    $resolved = [IO.Path]::GetFullPath($fixture)
    if ([IO.Path]::GetDirectoryName($resolved) -ne [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') -or [IO.Path]::GetFileName($resolved) -notlike 'cll-slots-*') { throw 'Unsafe cleanup path.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
