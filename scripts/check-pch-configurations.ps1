param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$RaylibHeaders,
    [Parameter(Mandatory=$true)][string]$ReportDirectory
)
$ErrorActionPreference = 'Stop'
$Executable = [IO.Path]::GetFullPath($Executable)
$RaylibHeaders = [IO.Path]::GetFullPath($RaylibHeaders)
$ReportDirectory = [IO.Path]::GetFullPath($ReportDirectory)
[IO.Directory]::CreateDirectory($ReportDirectory) | Out-Null
$fixture = Join-Path $env:TEMP ('cll-pch-' + [Guid]::NewGuid().ToString('N'))
if (Test-Path -LiteralPath $fixture) { throw 'Fixture already exists.' }
[IO.Directory]::CreateDirectory($fixture) | Out-Null
$oldState = $env:CPPLIVELAB_STATE_HOME
try {
    $env:CPPLIVELAB_STATE_HOME = Join-Path $fixture 'state'
    $source = Join-Path $fixture 'source'
    [IO.Directory]::CreateDirectory($source) | Out-Null
    $cmake = @'
cmake_minimum_required(VERSION 3.24)
project(PchSmoke LANGUAGES CXX)
add_executable(app main.cpp)
target_compile_features(app PRIVATE cxx_std_20)
target_include_directories(app PRIVATE "@HEADERS@")
set(PROJECT_NAME_SAFE app)
include("@PCH_SCRIPT@")
'@
    $snippet = Join-Path (Split-Path $PSScriptRoot -Parent) 'docs/experiments/auto-run/FarmGamePch.cmake'
    $cmake = $cmake.Replace('@HEADERS@',$RaylibHeaders.Replace('\','/')).Replace('@PCH_SCRIPT@',$snippet.Replace('\','/'))
    [IO.File]::WriteAllText((Join-Path $source 'CMakeLists.txt'),$cmake)
    [IO.File]::WriteAllText((Join-Path $source 'main.cpp'), @'
#include <raylib.h>
#include <string>
#include <vector>
#include <iostream>
int main()
{
    const Vector2 value{2, 3};
    const std::vector<std::string> text{"PCH configuration passed"};
    std::cout << text[0] << '\n';
    return value.x + value.y == 5 ? 0 : 1;
}
'@)
    foreach ($configuration in @('Debug','Release')) {
        $log = Join-Path $ReportDirectory "$configuration.log"
        $previous = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            & $Executable manage-run $source --target app --config $configuration *> $log
            $code = $LASTEXITCODE
        } finally { $ErrorActionPreference = $previous }
        $output = [IO.File]::ReadAllText($log)
        if ($code -ne 0 -or -not $output.Contains('PCH configuration passed') -or -not $output.Contains('cmake_pch')) {
            throw "PCH configuration failed: $configuration. See $log"
        }
        Write-Output "$configuration : PCH compile and execution passed"
    }
} finally {
    $env:CPPLIVELAB_STATE_HOME = $oldState
    $resolved = [IO.Path]::GetFullPath($fixture)
    if ([IO.Path]::GetDirectoryName($resolved) -ne [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') -or [IO.Path]::GetFileName($resolved) -notlike 'cll-pch-*') { throw 'Unsafe cleanup path.' }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
