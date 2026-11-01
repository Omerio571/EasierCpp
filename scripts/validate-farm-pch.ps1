param(
    [Parameter(Mandatory=$true)][string]$FarmGameDirectory,
    [Parameter(Mandatory=$true)][string]$BuildDirectory,
    [Parameter(Mandatory=$true)][string]$ReportDirectory,
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [ValidateSet('ON','OFF')][string]$Pch = 'ON'
)
$ErrorActionPreference = 'Stop'
$FarmGameDirectory = [IO.Path]::GetFullPath($FarmGameDirectory)
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$ReportDirectory = [IO.Path]::GetFullPath($ReportDirectory)
if (Test-Path -LiteralPath $BuildDirectory) { throw 'Use a fresh isolated build directory.' }
[IO.Directory]::CreateDirectory($ReportDirectory) | Out-Null
$hashes = @{}
foreach ($name in @('Source','Include','Assets')) {
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $FarmGameDirectory $name) -Recurse -File) {
        $hashes[$file.FullName] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    }
}
$hashes.GetEnumerator() | ForEach-Object { [pscustomobject]@{Path=$_.Key;Hash=$_.Value} } |
    Export-Csv (Join-Path $ReportDirectory 'source-hashes-before.csv') -NoTypeInformation
foreach ($name in @('Source','Include','Assets','Assets/Fonts','Assets/Sprites','Assets/Objects','Assets/Sounds','Assets/Data','Assets/Extra')) {
    if (-not (Test-Path -LiteralPath (Join-Path $FarmGameDirectory $name))) { throw 'Source-creating CMake commands require existing directories for this read-only-source validation.' }
}
$raylib = Join-Path $FarmGameDirectory 'out/build/x64-Debug/_deps/raylib-src'
if (-not (Test-Path -LiteralPath (Join-Path $raylib 'CMakeLists.txt'))) { throw 'Existing local raylib sources missing; no download is allowed.' }
$rows = [Collections.Generic.List[object]]::new()
function Invoke-Checked([string]$Case, [string[]]$Arguments) {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $previous = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $cmake @Arguments *> (Join-Path $ReportDirectory "$Case.log")
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previous }
    $rows.Add([pscustomobject]@{Case=$Case;Milliseconds=$watch.Elapsed.TotalMilliseconds;ExitCode=$code})
    if ($code -ne 0) { throw "Actual FarmGame validation failed: $Case. See report log." }
}
try {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $setup = Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
    $environment = & $env:ComSpec /d /c "call `"$setup`" -arch=x64 -host_arch=x64 >nul && set"
    if ($LASTEXITCODE -ne 0) { throw 'SDK setup failed.' }
    foreach ($line in $environment) {
        if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process') }
    }
    $cmake = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
    $ninja = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
    $compiler = Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    Invoke-Checked 'configure' @('-S',$FarmGameDirectory,'-B',$BuildDirectory,'-G','Ninja',
        "-DCMAKE_BUILD_TYPE=$Configuration","-DCMAKE_MAKE_PROGRAM=$ninja","-DCMAKE_CXX_COMPILER=$compiler",
        "-DCMAKE_C_COMPILER=$compiler","-DFARMGAME_USE_PCH=$Pch","-DFETCHCONTENT_SOURCE_DIR_RAYLIB=$raylib",
        '-DFETCHCONTENT_FULLY_DISCONNECTED=ON','-DFETCHCONTENT_UPDATES_DISCONNECTED=ON','-DCMAKE_POLICY_DEFAULT_CMP0168=NEW','-DBUILD_EXAMPLES=OFF')
    Invoke-Checked 'cold-build' @('--build',$BuildDirectory,'--parallel','4')
    $pchFiles = @(Get-ChildItem -LiteralPath $BuildDirectory -Recurse -Filter '*.pch')
    if ($Pch -eq 'ON' -and $pchFiles.Count -eq 0) { throw 'Requested PCH was not produced.' }
    $written = @{}
    foreach ($file in $pchFiles) { $written[$file.FullName] = $file.LastWriteTimeUtc }
    Invoke-Checked 'warm-noop' @('--build',$BuildDirectory,'--parallel','4')
    foreach ($path in $written.Keys) {
        if ([IO.File]::GetLastWriteTimeUtc($path) -ne $written[$path]) { throw 'No-op rebuilt PCH.' }
    }
    $assets = Join-Path $FarmGameDirectory 'Assets'
    foreach ($file in Get-ChildItem -LiteralPath $assets -File -Recurse) {
        $target = Join-Path (Join-Path $BuildDirectory 'Assets') $file.FullName.Substring($assets.Length+1)
        if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $hashes[$file.FullName]) { throw 'Runtime asset copy differs.' }
    }
    $rows | Export-Csv (Join-Path $ReportDirectory 'timings.csv') -NoTypeInformation
    $pchFiles | Select-Object Name,Length,LastWriteTimeUtc | Export-Csv (Join-Path $ReportDirectory 'pch-files.csv') -NoTypeInformation
    Select-String -Path (Join-Path $BuildDirectory 'CMakeCache.txt') -Pattern '^CMAKE_(CXX_FLAGS|EXE_LINKER_FLAGS)' |
        ForEach-Object { $_.Line } | Set-Content (Join-Path $ReportDirectory 'flags.txt')
    $rows | Format-Table
    Write-Output "Actual executable: $(Join-Path $BuildDirectory 'FarmGame.exe')"
} finally {
    foreach ($path in $hashes.Keys) {
        $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        if ($actual -ne $hashes[$path]) {
            "Path=$path; expected=$($hashes[$path]); actual=$actual" | Add-Content (Join-Path $ReportDirectory 'hash-mismatch.txt')
            throw "Original source/asset changed: $path"
        }
    }
}
