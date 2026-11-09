param(
    [Parameter(Mandatory=$true)][string]$Archive,
    [Parameter(Mandatory=$true)][string]$BuildDirectory,
    [Parameter(Mandatory=$true)][string]$ReportDirectory,
    [switch]$Performance
)
$ErrorActionPreference = 'Stop'
$Archive = [IO.Path]::GetFullPath($Archive)
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$ReportDirectory = [IO.Path]::GetFullPath($ReportDirectory)
[IO.Directory]::CreateDirectory($ReportDirectory) | Out-Null
$root = Join-Path $env:TEMP ('EasierPortable-' + [Guid]::NewGuid().ToString('N'))
$extracted = Join-Path $root 'Extracted package with spaces'
[IO.Directory]::CreateDirectory($root) | Out-Null
$originalHash = (Get-FileHash -LiteralPath $Archive -Algorithm SHA256).Hash
$checksumFile = $Archive + '.sha256'
if (-not (Test-Path -LiteralPath $checksumFile) -or
    [IO.File]::ReadAllText($checksumFile).Trim() -ne
        ($originalHash.ToLowerInvariant() + '  ' + [IO.Path]::GetFileName($Archive))) {
    throw 'Portable ZIP checksum file is missing or does not match.'
}
Expand-Archive -LiteralPath $Archive -DestinationPath $extracted
$bundle = @(Get-ChildItem -LiteralPath $extracted -Directory)[0].FullName
$cli = Join-Path $bundle 'EasierCpp.exe'
$env:CPPLIVELAB_TEST_CHILD = '1'
$env:CPPLIVELAB_STATE_HOME = Join-Path $root 'isolated manager state'
# Test ordinary-terminal discovery. Keep changes in this test process only.
$env:PATH = "$env:SystemRoot\System32;$env:SystemRoot;$env:SystemRoot\System32\WindowsPowerShell\v1.0"
foreach ($name in @('CPPLIVELAB_PREPARED_VS','VSINSTALLDIR','VCINSTALLDIR','VCToolsInstallDir',
    'INCLUDE','LIB','LIBPATH','VSCMD_ARG_TGT_ARCH','VSCMD_ARG_HOST_ARCH')) {
    [Environment]::SetEnvironmentVariable($name, $null, 'Process')
}
$passed = [Collections.Generic.List[string]]::new()
$succeeded = $false
$passed.Add('zip-checksum')
function Run-Check($name, $executable, $arguments) {
    $log = Join-Path $ReportDirectory "$name.log"
    $previous = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $executable @arguments *> $log
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previous }
    if ($code) { throw "Packaged check failed: $name. See $log" }
    $passed.Add($name)
}
try {
    Push-Location $root
    try {
        Run-Check version $cli @('--version')
        $version = [IO.File]::ReadAllText((Join-Path $BuildDirectory 'release-version.txt')).Trim()
        if ([IO.File]::ReadAllText((Join-Path $ReportDirectory 'version.log')).Trim() -ne "EasierCpp $version") { throw 'Package version mismatch.' }
        foreach ($name in @('EasierCpp.exe','EasierCpp-Gui.exe')) {
            $metadata = [Diagnostics.FileVersionInfo]::GetVersionInfo((Join-Path $bundle $name))
            if ($metadata.FileVersion -ne $version -or $metadata.ProductVersion -ne $version) {
                throw "Windows version metadata mismatch: $name"
            }
        }
        $passed.Add('windows-version-metadata')
        # Scan only pristine extracted runtime files, before adding test controllers.
        # No environment values or matching secret bytes are written to the log.
        $privatePath = [Environment]::GetFolderPath('UserProfile')
        foreach ($file in Get-ChildItem -LiteralPath $bundle -File -Recurse) {
            $bytes = [IO.File]::ReadAllBytes($file.FullName)
            foreach ($encoding in @([Text.Encoding]::UTF8, [Text.Encoding]::Unicode)) {
                $text = $encoding.GetString($bytes)
                if ($text.IndexOf($privatePath, [StringComparison]::OrdinalIgnoreCase) -ge 0 -or
                    $text.IndexOf($privatePath.Replace('\','/'), [StringComparison]::OrdinalIgnoreCase) -ge 0 -or
                    $text -match '-----BEGIN (RSA |EC |OPENSSH )?PRIVATE KEY-----|github_pat_[A-Za-z0-9_]{30,}|gh[pousr]_[A-Za-z0-9]{30,}|glpat-[A-Za-z0-9_-]{20,}') {
                    throw "Private path or credential signature found in $($file.Name)."
                }
            }
        }
        $passed.Add('private-path-and-credential-signatures')
        Run-Check doctor $cli @('doctor')
        $doctor = [IO.File]::ReadAllText((Join-Path $ReportDirectory 'doctor.log'))
        if ($doctor -notmatch 'cl: Installed: Yes \| Available in current terminal: No') { throw 'Doctor did not verify ordinary-terminal discovery.' }
        $project = Join-Path $root 'Created console project'
        Run-Check create $cli @('create', $project, '--demo', 'calculator')
        foreach ($configuration in @('Debug','Release')) {
            Run-Check "build-$configuration" $cli @('manage-build', $project, '--target', 'app', '--config', $configuration)
            $log = Join-Path $ReportDirectory "run-$configuration.log"
            @('Ada','2','3') | & $cli manage-run $project --target app --config $configuration *> $log
            if ($LASTEXITCODE -or [IO.File]::ReadAllText($log) -notmatch 'Hello, Ada! Sum: 5') { throw 'Packaged calculator input/output failed.' }
            $passed.Add("run-$configuration")
        }
        $discovery = Join-Path $root 'Discovered source project'
        Run-Check create-standard $cli @('new', $discovery, '--template', 'multi')
        $ignore = [IO.File]::ReadAllText((Join-Path $discovery '.gitignore'))
        if ($ignore -notmatch '/build/' -or $ignore -notmatch 'CMakeUserPresets.json' -or
            -not (Test-Path -LiteralPath (Join-Path $discovery 'README.md')) -or
            -not (Test-Path -LiteralPath (Join-Path $discovery 'assets')) -or
            (Test-Path -LiteralPath (Join-Path $discovery '.git'))) {
            throw 'Packaged standard-project layout or ignore defaults are incorrect.'
        }
        $passed.Add('generated-layout-and-gitignore')
        $nested = Join-Path $discovery 'src/game'
        [IO.Directory]::CreateDirectory($nested) | Out-Null
        [IO.File]::WriteAllText((Join-Path $discovery 'include/Game.hpp'),
            "#pragma once`nint game();`n")
        [IO.File]::WriteAllText((Join-Path $nested 'Game.cpp'),
            "#include `"Game.hpp`"`nint game() { return 42; }`n")
        [IO.File]::WriteAllText((Join-Path $discovery 'src/main.cpp'),
            "#include `"Game.hpp`"`nint main() { return game() == 42 ? 0 : 1; }`n")
        $originalCMake = [IO.File]::ReadAllText((Join-Path $discovery 'CMakeLists.txt'))
        foreach ($configuration in @('Debug','Release')) {
            Run-Check "discover-$configuration" $cli @('manage-run', $discovery, '--config', $configuration)
        }
        Rename-Item -LiteralPath (Join-Path $nested 'Game.cpp') -NewName 'Renamed.cc'
        Run-Check discover-rename $cli @('manage-run', $discovery, '--incremental')
        [IO.File]::WriteAllText((Join-Path $nested 'Unused.cpp'), "int unused() { return 1; }`n")
        Run-Check discover-add $cli @('manage-build', $discovery, '--incremental')
        Remove-Item -LiteralPath (Join-Path $nested 'Unused.cpp')
        Run-Check discover-remove $cli @('manage-run', $discovery, '--incremental')
        if ([IO.File]::ReadAllText((Join-Path $discovery 'CMakeLists.txt')) -cne $originalCMake) {
            throw 'Source discovery rewrote the project configuration.'
        }
        # Absence of support files must fail even while the checkout is accessible.
        Rename-Item -LiteralPath (Join-Path $bundle 'resources') -NewName 'resources-unavailable'
        try {
            $previous = $ErrorActionPreference
            try {
                $ErrorActionPreference = 'Continue'
                & $cli manage-build $project --target app --incremental *> (Join-Path $ReportDirectory 'missing-resources.log')
                $code = $LASTEXITCODE
            } finally { $ErrorActionPreference = $previous }
            if ($code -eq 0 -or [IO.File]::ReadAllText((Join-Path $ReportDirectory 'missing-resources.log')) -notmatch 'Re-extract the complete ZIP') { throw 'Portable build unexpectedly used checkout fallback.' }
            $passed.Add('missing-resources-no-source-fallback')
        } finally {
            Rename-Item -LiteralPath (Join-Path $bundle 'resources-unavailable') -NewName 'resources'
        }
        # The native controller lives in the Development build. It launches the
        # actual public GUI and checks absent test controls, themes, persistence,
        # inline forms, console input, Auto Run, Stop and Restart.
        Run-Check actual-public-gui (Join-Path $BuildDirectory 'restart_gui_tests.exe') @((Join-Path $bundle 'EasierCpp-Gui.exe'), '--public')
        Copy-Item -LiteralPath (Join-Path $BuildDirectory 'library_management_tests.exe') -Destination (Join-Path $bundle 'library_management_tests.exe')
        Run-Check packaged-local-library-consumer (Join-Path $bundle 'library_management_tests.exe') @('consumers')
        if ($Performance) {
            Copy-Item -LiteralPath (Join-Path $BuildDirectory 'gui_flow_tests.exe') -Destination (Join-Path $bundle 'gui_flow_tests.exe')
            # Benchmark controllers stay outside the archive. Their children use only
            # this extracted CLI/resources, ordinary-terminal discovery and isolated state.
            $benchmark = Join-Path $root 'Owned portable performance fixture'
            [IO.Directory]::CreateDirectory((Join-Path $benchmark 'Source')) | Out-Null
            [IO.Directory]::CreateDirectory((Join-Path $benchmark 'Include')) | Out-Null
            Set-Content -LiteralPath (Join-Path $benchmark '.easiercpp-benchmark') 'EasierCpp owned Phase 3 copy'
            Set-Content -LiteralPath (Join-Path $benchmark 'Include/Config.h') '#pragma once'
            $nativeSample = Join-Path (Split-Path $PSScriptRoot -Parent) 'tests/consumers/native_window.cpp'
            $nativeSource = '#include "Config.h"' + "`n" + [IO.File]::ReadAllText($nativeSample)
            [IO.File]::WriteAllText((Join-Path $benchmark 'Source/main.cpp'), $nativeSource)
            @'
int dependency_value()
{
    return 42;
}
'@ | Set-Content -LiteralPath (Join-Path $benchmark 'Source/dependency.cpp')
        @'
cmake_minimum_required(VERSION 3.24)
project(PortablePerformance LANGUAGES CXX)
add_library(dependency STATIC Source/dependency.cpp)
target_compile_features(dependency PUBLIC cxx_std_20)
target_precompile_headers(dependency PRIVATE <string> <vector>)
add_executable(app Source/main.cpp)
target_compile_features(app PRIVATE cxx_std_20)
target_include_directories(app PRIVATE Include)
target_precompile_headers(app PRIVATE <string> <vector>)
target_link_libraries(app PRIVATE dependency user32)
'@ | Set-Content -LiteralPath (Join-Path $benchmark 'CMakeLists.txt')
        Run-Check packaged-two-tree-performance (Join-Path $bundle 'gui_flow_tests.exe') @('--benchmark-project', $benchmark, '5', '150')
        Run-Check packaged-replacement-performance (Join-Path $bundle 'gui_flow_tests.exe') @('--benchmark-replacement')
        }
        $files = @(Get-ChildItem -LiteralPath $bundle -File -Recurse | ForEach-Object {
            $_.FullName.Substring($bundle.Length + 1).Replace('\','/')
        })
        $files | Set-Content (Join-Path $ReportDirectory 'extracted-test-file-inventory.txt')
        if ((Get-FileHash -LiteralPath $Archive -Algorithm SHA256).Hash -ne $originalHash) { throw 'Acceptance testing modified the ZIP.' }
        $passed | Set-Content (Join-Path $ReportDirectory 'passed.txt')
        $succeeded = $true
        Write-Output "All $($passed.Count) packaged acceptance checks passed. ZIP unchanged."
    } finally { Pop-Location }
} finally {
    $passed | Set-Content (Join-Path $ReportDirectory 'completed-checks.txt')
    # Preserve a failed fixture for diagnosis; successful cleanup is explicitly owned.
    if ($succeeded) {
        $absolute = [IO.Path]::GetFullPath($root)
        if ([IO.Path]::GetDirectoryName($absolute) -ne [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') -or
            [IO.Path]::GetFileName($absolute) -notmatch '^EasierPortable-[0-9a-f]{32}$') { throw 'Unsafe portable fixture cleanup path.' }
        Remove-Item -LiteralPath $absolute -Recurse -Force
    } else {
        Write-Output "Failed portable fixture retained for diagnosis: $root"
    }
}
