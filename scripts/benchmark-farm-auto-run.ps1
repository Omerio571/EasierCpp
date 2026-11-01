param(
    [Parameter(Mandatory=$true)][string]$ExecutableDirectory,
    [Parameter(Mandatory=$true)][string]$FarmGameDirectory,
    [Parameter(Mandatory=$true)][string]$ReportDirectory,
    [ValidateRange(3,30)][int]$Samples = 10,
    [switch]$PrecompiledHeaders,
    [switch]$IncrementalAssets,
    [switch]$FullLink,
    [switch]$ProjectCmake
)
$ErrorActionPreference = 'Stop'
$ExecutableDirectory = [IO.Path]::GetFullPath($ExecutableDirectory)
$FarmGameDirectory = [IO.Path]::GetFullPath($FarmGameDirectory)
$ReportDirectory = [IO.Path]::GetFullPath($ReportDirectory)
[IO.Directory]::CreateDirectory($ReportDirectory) | Out-Null
$fixture = Join-Path $env:TEMP ('cll-p3-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
if (Test-Path -LiteralPath $fixture) { throw 'Fixture already exists.' }
$project = Join-Path $fixture 'FarmGame'
$originalHashes = @{}
# Deliberately copy only project inputs, not .git/.vs/out/dependency build trees.
$inputs = @((Get-Item (Join-Path $FarmGameDirectory 'CMakeLists.txt')))
foreach ($name in @('Source','Include','Assets')) {
    $inputs += Get-ChildItem -LiteralPath (Join-Path $FarmGameDirectory $name) -Recurse -File
}
if (Test-Path -LiteralPath (Join-Path $FarmGameDirectory 'cmake')) {
    $inputs += Get-ChildItem -LiteralPath (Join-Path $FarmGameDirectory 'cmake') -Recurse -File
}
foreach ($file in $inputs) {
    if ($file.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Reparse-point input refused.' }
    $originalHashes[$file.FullName] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
}
$cached = Join-Path $FarmGameDirectory 'out/build/x64-Debug/_deps/raylib-build/raylib'
$library = Join-Path $cached 'raylib.lib'
$headers = Join-Path $cached 'include'
if (-not (Test-Path -LiteralPath $library) -or -not (Test-Path -LiteralPath (Join-Path $headers 'raylib.h'))) {
    throw 'Expected existing local Debug raylib binary/headers are unavailable. No download is attempted.'
}
try {
    foreach ($file in $inputs) {
        $relative = $file.FullName.Substring($FarmGameDirectory.TrimEnd('\').Length + 1)
        $destination = Join-Path $project $relative
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $destination
    }
    [IO.File]::WriteAllText((Join-Path $project '.easiercpp-benchmark'), "EasierCpp owned Phase 3 copy`n")
    # Equivalent local consumer: original sources/headers/assets and system link
    # libraries, with the already-built raylib instead of executing FetchContent.
    # The original source-creating CMake commands are unnecessary in this complete copy.
    $cmake = @'
cmake_minimum_required(VERSION 3.24)
project(FarmGame LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_library(raylib STATIC IMPORTED)
set_target_properties(raylib PROPERTIES
    IMPORTED_LOCATION "@LIBRARY@"
    INTERFACE_INCLUDE_DIRECTORIES "@HEADERS@"
)
file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp")
add_executable(FarmGame ${sources})
target_include_directories(FarmGame PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Include")
target_link_libraries(FarmGame PRIVATE raylib opengl32 glu32 winmm)
target_link_options(FarmGame PRIVATE /VERBOSE:INCR)
set(asset_copy copy_directory)
if(@INCREMENTAL_ASSETS@ AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.26)
    set(asset_copy copy_directory_if_different)
endif()
add_custom_command(TARGET FarmGame POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E ${asset_copy}
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets" "$<TARGET_FILE_DIR:FarmGame>/Assets"
    VERBATIM
)
'@
    $cmake = $cmake.Replace('@LIBRARY@',$library.Replace('\','/')).Replace('@HEADERS@',$headers.Replace('\','/'))
    $cmake = $cmake.Replace('@INCREMENTAL_ASSETS@',$(if ($IncrementalAssets) { 'ON' } else { 'OFF' }))
    if ($ProjectCmake) {
        $cmake = [IO.File]::ReadAllText((Join-Path $FarmGameDirectory 'CMakeLists.txt'))
        $population = 'FetchContent_MakeAvailable(raylib)'
        if (-not $cmake.Contains($population)) { throw 'Expected raylib population statement not found.' }
        $import = "add_library(raylib STATIC IMPORTED)`nset_target_properties(raylib PROPERTIES IMPORTED_LOCATION `"$($library.Replace('\','/'))`" INTERFACE_INCLUDE_DIRECTORIES `"$($headers.Replace('\','/'))`")`nset_property(TARGET raylib PROPERTY INTERFACE_LINK_LIBRARIES opengl32 glu32 winmm)`n"
        $import += "set(FARMGAME_USE_PCH $(if ($PrecompiledHeaders) { 'ON' } else { 'OFF' }) CACHE BOOL `"Benchmark configuration`" FORCE)`n"
        $cmake = $cmake.Replace($population,$import)
        $cmake += "`ntarget_link_options(FarmGame PRIVATE /VERBOSE:INCR)`n"
    } elseif ($PrecompiledHeaders) {
        $cmake += "`ntarget_precompile_headers(FarmGame PRIVATE <string> <vector> <iostream> <cmath> <raylib.h> <raymath.h>)`n"
    }
    if ($FullLink) { $cmake += "`ntarget_link_options(FarmGame PRIVATE /INCREMENTAL:NO)`n" }
    [IO.File]::WriteAllText((Join-Path $project 'CMakeLists.txt'), $cmake)
    Copy-Item -LiteralPath (Join-Path $project 'CMakeLists.txt') -Destination (Join-Path $ReportDirectory 'fixture-CMakeLists.txt')
    $log = Join-Path $ReportDirectory 'gui.log'
    $previousErrorAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & (Join-Path $ExecutableDirectory 'gui_flow_tests.exe') --benchmark-project $project $Samples *> $log
        $result = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousErrorAction }
    if ($result -ne 0) { throw "FarmGame benchmark failed ($result). See $log" }
    $lines = [IO.File]::ReadAllLines($log)
    $profiles = @{}
    $workerStarts = @{}
    foreach ($line in $lines) {
        if ($line -match '^FARM_PROFILE,([^,]+),([^=]+)=([0-9.]+)') {
            if (-not $profiles.ContainsKey($matches[1])) { $profiles[$matches[1]] = [ordered]@{Sample=$matches[1]} }
            $profiles[$matches[1]][$matches[2]] = [double]::Parse($matches[3], [Globalization.CultureInfo]::InvariantCulture)
        } elseif ($line -match '^FARM_WORKER_START,([^,]+),([0-9]+)') {
            $workerStarts[$matches[1]] = [double]$matches[2]
        }
    }
    foreach ($sample in $workerStarts.Keys) {
        $profiles[$sample]['worker_startup_ms'] = $profiles[$sample]['worker_start_tick'] - $workerStarts[$sample]
    }
    $objects = @($profiles.Values | ForEach-Object { [pscustomobject]$_ })
    $columns = $objects | ForEach-Object { $_.PSObject.Properties.Name } | Select-Object -Unique
    $objects | Select-Object $columns | Export-Csv (Join-Path $ReportDirectory 'profiles.csv') -NoTypeInformation
    $timing = @($lines | Where-Object { $_.StartsWith('FARM_TIMING,') } | ForEach-Object { $_.Substring(12) })
    $timing | ConvertFrom-Csv | Export-Csv (Join-Path $ReportDirectory 'timings.csv') -NoTypeInformation
    $rows = $timing | ConvertFrom-Csv | Where-Object sample -ne 'header'
    $summary = foreach ($column in @('total','build','peak_worker_bytes')) {
        $values = @($rows | ForEach-Object { [double]$_.$column } | Sort-Object)
        $middle = [int][Math]::Floor($values.Count/2)
        $median = if ($values.Count % 2) { $values[$middle] } else { ($values[$middle-1]+$values[$middle])/2 }
        [pscustomobject]@{Metric=$column;Samples=$values.Count;Median=$median;P95=$values[[int][Math]::Ceiling($values.Count*0.95)-1];Maximum=$values[-1]}
    }
    $summary | Export-Csv (Join-Path $ReportDirectory 'summary.csv') -NoTypeInformation
    $summary | Format-Table
} finally {
    foreach ($path in $originalHashes.Keys) {
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $originalHashes[$path]) {
            throw "Original project changed during benchmark: $path"
        }
    }
    $resolved = [IO.Path]::GetFullPath($fixture)
    if ([IO.Path]::GetDirectoryName($resolved) -ne [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') -or [IO.Path]::GetFileName($resolved) -notlike 'cll-p3-*') {
        throw 'Unsafe cleanup path.'
    }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
