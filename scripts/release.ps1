param(
    [string]$BuildDirectory = 'build/portable-release',
    [string]$OutputDirectory = 'dist',
    [ValidateRange(1, 4)][int]$Jobs = 1
)
$ErrorActionPreference = 'Stop'
$repository = Split-Path $PSScriptRoot -Parent
$BuildDirectory = [IO.Path]::GetFullPath($(if ([IO.Path]::IsPathRooted($BuildDirectory)) {
    $BuildDirectory
} else { Join-Path $repository $BuildDirectory }))
$OutputDirectory = [IO.Path]::GetFullPath($(if ([IO.Path]::IsPathRooted($OutputDirectory)) {
    $OutputDirectory
} else { Join-Path $repository $OutputDirectory }))
if (Test-Path -LiteralPath $OutputDirectory) {
    throw 'Release output already exists. Choose a new OutputDirectory; existing artifacts are never overwritten.'
}
$cache = Join-Path $BuildDirectory 'CMakeCache.txt'
if (Test-Path -LiteralPath $cache) {
    $cacheText = [IO.File]::ReadAllText($cache)
    $sourceMatch = [regex]::Match($cacheText, '(?m)^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)\r?$')
    if ($cacheText -notmatch '(?m)^CMAKE_BUILD_TYPE:STRING=Release\r?$' -or
        $cacheText -notmatch '(?m)^EASIERCPP_PORTABLE:BOOL=ON\r?$' -or
        -not $sourceMatch.Success -or
        [IO.Path]::GetFullPath($sourceMatch.Groups[1].Value.Trim()) -ne [IO.Path]::GetFullPath($repository)) {
        throw 'BuildDirectory belongs to another project/configuration. Choose a fresh Release directory; development builds are preserved.'
    }
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Install the Visual Studio Desktop development with C++ workload.' }
$setup = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
# Only this release-script process and its children receive the SDK environment.
$environment = & $env:ComSpec /d /c "call `"$setup`" -arch=x64 -host_arch=x64 >nul && set"
if ($LASTEXITCODE) { throw 'Developer environment setup failed.' }
foreach ($line in $environment) {
    if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
}
$cmake = Join-Path $visualStudio 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ninja = Join-Path $visualStudio 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
& $cmake -S $repository -B $BuildDirectory -G Ninja -DCMAKE_BUILD_TYPE=Release -DEASIERCPP_PORTABLE=ON -DBUILD_TESTING=OFF "-DCMAKE_MAKE_PROGRAM=$ninja"
if ($LASTEXITCODE) { throw 'Release configuration failed.' }
& $cmake --build $BuildDirectory --parallel $Jobs
if ($LASTEXITCODE) { throw 'Release build failed.' }
$version = [IO.File]::ReadAllText((Join-Path $BuildDirectory 'release-version.txt')).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw 'Invalid CMake project version.' }
$folder = Join-Path $OutputDirectory "EasierCpp-v$version"
[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
& $cmake --install $BuildDirectory --prefix $folder --config Release --component Runtime
if ($LASTEXITCODE) { throw 'Release staging failed; output is not a release candidate.' }
$expected = @('EasierCpp.exe', 'EasierCpp-Gui.exe', 'LICENSE', 'THIRD_PARTY_NOTICES.md',
    'README.md', 'RELEASE_NOTES.md', 'resources/scripts/file_api.cmake',
    'resources/scripts/discover_targets.cmake', 'resources/scripts/github-library.ps1',
    'resources/scripts/detect-project-libraries.ps1', 'resources/cmake/EasierCppPch.cmake',
    'resources/cmake/EasierCppPchHook.cmake', 'resources/branding/EasierCpp.ico',
    'resources/branding/EasierCpp.svg') | Sort-Object
$actual = @(Get-ChildItem -LiteralPath $folder -File -Recurse | ForEach-Object {
    $_.FullName.Substring($folder.Length + 1).Replace('\', '/')
} | Sort-Object)
if (Compare-Object $expected $actual -CaseSensitive) { throw 'Staging contains unexpected or missing files.' }
foreach ($name in @('EasierCpp.exe', 'EasierCpp-Gui.exe')) {
    $info = (Get-Item -LiteralPath (Join-Path $folder $name)).VersionInfo
    if ($info.FileVersion -ne $version -or $info.ProductVersion -ne $version) {
        throw "Version metadata in $name does not match the CMake project version."
    }
}
# A portable binary must not retain this machine's checkout path in either encoding.
foreach ($file in Get-ChildItem -LiteralPath $folder -File -Recurse) {
    $bytes = [IO.File]::ReadAllBytes($file.FullName)
    foreach ($encoding in @([Text.Encoding]::UTF8, [Text.Encoding]::Unicode)) {
        $text = $encoding.GetString($bytes)
        if ($text.IndexOf($repository, [StringComparison]::OrdinalIgnoreCase) -ge 0 -or
            $text.IndexOf($repository.Replace('\','/'), [StringComparison]::OrdinalIgnoreCase) -ge 0) {
            throw "Machine-specific checkout path found in $($file.Name); package refused."
        }
    }
}
$archive = Join-Path $OutputDirectory "EasierCpp-v$version-win-x64.zip"
Compress-Archive -LiteralPath $folder -DestinationPath $archive -CompressionLevel Optimal
$hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText($archive + '.sha256', "$hash  $([IO.Path]::GetFileName($archive))`n")
Write-Output "Prepared local candidate for packaged acceptance testing: $archive"
Write-Output "SHA256: $hash"
