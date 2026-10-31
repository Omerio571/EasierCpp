param(
    [Parameter(Mandatory=$true)][string]$AssetsDirectory,
    [Parameter(Mandatory=$true)][string]$ReportDirectory,
    [ValidateRange(3,30)][int]$Samples = 10
)
$ErrorActionPreference = 'Stop'
$AssetsDirectory = [IO.Path]::GetFullPath($AssetsDirectory)
$ReportDirectory = [IO.Path]::GetFullPath($ReportDirectory)
[IO.Directory]::CreateDirectory($ReportDirectory) | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$cmake = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$fixture = Join-Path $env:TEMP ('cll-assets-' + [Guid]::NewGuid().ToString('N'))
if (Test-Path -LiteralPath $fixture) { throw 'Fixture already exists.' }
$rows = [Collections.Generic.List[object]]::new()
try {
    $version = & $cmake --version
    if ($LASTEXITCODE -ne 0) { throw 'CMake unavailable.' }
    $version | Set-Content (Join-Path $ReportDirectory 'cmake-version.txt')
    foreach ($mode in @('copy_directory','copy_directory_if_different')) {
        $source = Join-Path $fixture "$mode/source"
        $destination = Join-Path $fixture "$mode/destination"
        [IO.Directory]::CreateDirectory($source) | Out-Null
        Get-ChildItem -LiteralPath $AssetsDirectory | Copy-Item -Destination $source -Recurse
        $copy = {
            & $cmake -E $mode $source $destination
            if ($LASTEXITCODE -ne 0) { throw "Asset copy failed: $mode" }
        }
        & $copy
        $asset = Get-ChildItem -LiteralPath $source -Recurse -File | Select-Object -First 1
        if (-not $asset) { throw 'Expected representative assets.' }
        $relative = $asset.FullName.Substring($source.Length + 1)
        $target = Join-Path $destination $relative
        $old = [DateTime]::UtcNow.AddDays(-1)
        [IO.File]::SetLastWriteTimeUtc($target,$old)
        for ($sample=0; $sample -lt $Samples; $sample++) {
            $watch = [Diagnostics.Stopwatch]::StartNew()
            & $copy
            $rows.Add([pscustomobject]@{Mode=$mode;Sample=$sample;Milliseconds=$watch.Elapsed.TotalMilliseconds})
        }
        $retained = [IO.File]::GetLastWriteTimeUtc($target) -eq $old
        if ($mode -eq 'copy_directory_if_different' -and -not $retained) { throw 'Unchanged asset was rewritten.' }
        [IO.File]::WriteAllText($asset.FullName, 'changed fixture bytes')
        [IO.File]::WriteAllText((Join-Path $source 'new-asset.txt'), 'new fixture bytes')
        & $copy
        if ([IO.File]::ReadAllText($target) -ne 'changed fixture bytes' -or
            [IO.File]::ReadAllText((Join-Path $destination 'new-asset.txt')) -ne 'new fixture bytes') { throw 'Changed/new assets were lost.' }
        Remove-Item -LiteralPath (Join-Path $source 'new-asset.txt')
        & $copy
        # Both CMake commands merge directories; neither mirrors source deletions.
        if (-not (Test-Path -LiteralPath (Join-Path $destination 'new-asset.txt'))) { throw 'Unexpected deletion behavior.' }
        "$mode : unchanged timestamp retained=$retained; changed/new=passed; deleted source=destination retained" |
            Add-Content (Join-Path $ReportDirectory 'checks.txt')
    }
    $rows | Export-Csv (Join-Path $ReportDirectory 'samples.csv') -NoTypeInformation
    $rows | Group-Object Mode | ForEach-Object {
        $values = @($_.Group.Milliseconds | Sort-Object)
        $middle = [int][Math]::Floor($values.Count/2)
        $median = if ($values.Count % 2) { $values[$middle] } else { ($values[$middle-1]+$values[$middle])/2 }
        [pscustomobject]@{Mode=$_.Name;Median=$median;P95=$values[[int][Math]::Ceiling($values.Count*0.95)-1];Maximum=$values[-1]}
    } | Export-Csv (Join-Path $ReportDirectory 'summary.csv') -NoTypeInformation
    Get-Content (Join-Path $ReportDirectory 'checks.txt')
    Import-Csv (Join-Path $ReportDirectory 'summary.csv') | Format-Table
} finally {
    $resolved = [IO.Path]::GetFullPath($fixture)
    if ([IO.Path]::GetDirectoryName($resolved) -ne [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') -or [IO.Path]::GetFileName($resolved) -notlike 'cll-assets-*') { throw 'Unsafe cleanup path.' }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
