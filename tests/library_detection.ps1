param([Parameter(Mandatory=$true)][string]$Script)
$ErrorActionPreference = 'Stop'
$root = Join-Path $env:TEMP ('EasierCPP-detection-' + [Guid]::NewGuid().ToString('N'))
$project = Join-Path $root 'project'
$build = Join-Path $root 'build'
$reply = Join-Path $build '.cmake/api/v1/reply'
[IO.Directory]::CreateDirectory($project) | Out-Null
[IO.Directory]::CreateDirectory($reply) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
function Write-Text($path, $text) { [IO.File]::WriteAllText($path, $text, $utf8) }
function Write-Json($path, $value) { Write-Text $path ($value | ConvertTo-Json -Depth 20) }
function Require([bool]$value, $message) { if (-not $value) { throw $message } }
function Fingerprint($directory) {
    return ((Get-ChildItem -LiteralPath $directory -File -Recurse | Sort-Object FullName | ForEach-Object {
        $_.FullName + ':' + (Get-FileHash -LiteralPath $_.FullName).Hash
    }) -join "`n")
}
function Inspect($target = '') {
    & $Script -Project $project -Build $build -Target $target -Output (Join-Path $root 'observations.txt')
    $lines = [IO.File]::ReadAllLines((Join-Path $root 'observations.txt'))
    Require ($lines.Count % 8 -eq 0) 'Incomplete observation stream'
    $result = @()
    for ($i = 0; $i -lt $lines.Count; $i += 8) {
        $result += [pscustomobject]@{id=$lines[$i]; name=$lines[$i+1]; source=$lines[$i+2]; version=$lines[$i+3]; ownership=$lines[$i+4]; evidence=$lines[$i+5]; target=$lines[$i+6]; confidence=$lines[$i+7]}
    }
    return $result
}
try {
    $cmake = Join-Path $project 'CMakeLists.txt'
    Write-Text $cmake @'
# find_package(CommentedOut)
#[[ find_package(BracketComment) ]]
find_package(Example 1.0 CONFIG)
find_package(Example 1.0 CONFIG)
if(FALSE)
    FetchContent_Declare(ConditionalDependency URL unused)
endif()
add_subdirectory(vendor/local)
'@
    $original = [IO.File]::ReadAllText($cmake)
    $observations = @(Inspect)
    Require ($observations.Count -eq 3) 'Literal hints were missing, duplicated, or included comments'
    Require (@($observations | Where-Object { $_.ownership -ne 'Unverified' -or $_.version -ne 'Unknown' }).Count -eq 0) 'Unevaluated dependency was confirmed'
    Require (@($observations | Where-Object name -eq ConditionalDependency).Count -eq 1) 'Conditional hint missing'
    Write-Json (Join-Path $project 'vcpkg.json') @{dependencies=@('fmt', @{name='zlib'; 'version>='='1.2'})}
    $observations = @(Inspect)
    Require (@($observations | Where-Object source -eq 'vcpkg manifest').Count -eq 2) 'vcpkg manifest detection failed'
    Require (@($observations | Where-Object { $_.name -eq 'zlib' -and $_.version -eq 'Unknown' }).Count -eq 1) 'Declared constraint became an installed version'
    Write-Text (Join-Path $build 'CMakeCache.txt') ''
    $model = @{paths=@{source=$project; build=$build}; configurations=@(@{name='Debug'; directories=@(); targets=@(
        @{id='app'; jsonFile='app.json'}, @{id='middle'; jsonFile='middle.json'},
        @{id='core'; jsonFile='core.json'}, @{id='unused'; jsonFile='unused.json'})})}
    Write-Json (Join-Path $reply 'model.json') $model
    Write-Json (Join-Path $reply 'app.json') @{name='app'; type='EXECUTABLE'; dependencies=@(@{id='middle'}, @{id='core'})}
    Write-Json (Join-Path $reply 'middle.json') @{name='middle'; type='STATIC_LIBRARY'; dependencies=@(@{id='core'})}
    Write-Json (Join-Path $reply 'core.json') @{name='core'; type='STATIC_LIBRARY'; paths=@{source='vendor/core'}}
    Write-Json (Join-Path $reply 'unused.json') @{name='unused'; type='STATIC_LIBRARY'}
    Write-Json (Join-Path $reply 'index-fixture.json') @{objects=@(@{kind='codemodel'; version=@{major=2}; jsonFile='model.json'})}
    $before = Fingerprint $project
    $buildBefore = Fingerprint $build
    $observations = @(Inspect 'app')
    Require ((Fingerprint $project) -eq $before -and (Fingerprint $build) -eq $buildBefore) 'Passive inspection wrote project/build files'
    $targets = @($observations | Where-Object source -eq 'CMake File API')
    Require ($targets.Count -eq 2 -and @($targets | Where-Object name -eq unused).Count -eq 0) 'Target closure missed transitive dependencies or included unused target'
    Require (@($targets | Where-Object ownership -eq External).Count -eq 2) 'Configured targets not recognized'
    Require (@($observations | Where-Object name -eq Example).Count -eq 1) 'find_package hint missing on older codemodel'
    $model.configurations[0].abstractTargets = @(@{id='headers'; jsonFile='headers.json'})
    Write-Json (Join-Path $reply 'headers.json') @{name='Headers::API'; type='INTERFACE_LIBRARY'; imported=$true}
    Write-Json (Join-Path $reply 'model.json') $model
    Write-Json (Join-Path $reply 'app.json') @{name='app'; type='EXECUTABLE'; dependencies=@(@{id='middle'}, @{id='core'}, @{id='headers'})}
    $observations = @(Inspect 'app')
    Require (@($observations | Where-Object target -eq 'Headers::API').Count -eq 1) 'Structured imported/header-only target was omitted'
    $release = @()
    & $Script -Project $project -Build $build -Configuration Release -Output (Join-Path $root 'release.txt')
    $release = [IO.File]::ReadAllText((Join-Path $root 'release.txt'))
    Require ($release -notmatch 'existing configured snapshot') 'Debug evidence was displayed as Release'
    [IO.File]::SetLastWriteTimeUtc($cmake, [DateTime]::UtcNow.AddSeconds(5))
    $observations = @(Inspect)
    Require (@($observations | Where-Object { $_.source -eq 'CMake File API' -and $_.confidence -eq 'ConfiguredSnapshot' }).Count -eq 0) 'Stale configuration was shown as current'
    Require (@($observations | Where-Object { $_.target -eq 'core' -and $_.ownership -eq 'External' -and $_.confidence -eq 'StaleSnapshot' }).Count -eq 1) 'Staleness incorrectly changed external ownership'
    Require ([IO.File]::ReadAllText($cmake) -eq $original) 'Detection modified user CMake'
    Write-Text (Join-Path $reply 'model.json') '{bad json'
    $observations = @(Inspect)
    Require (@($observations | Where-Object id -eq metadata-error).Count -eq 1) 'Malformed metadata was not reported safely'
    Write-Output 'Read-only detection fixtures passed: declarations, conditionals, vcpkg, configured closure, duplicates, configs, stale/malformed metadata.'
} finally {
    $absolute = [IO.Path]::GetFullPath($root)
    if ([IO.Path]::GetDirectoryName($absolute) -ne [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') -or
        [IO.Path]::GetFileName($absolute) -notmatch '^EasierCPP-detection-[0-9a-f]{32}$') { throw 'Unsafe fixture cleanup path.' }
    Remove-Item -LiteralPath $absolute -Recurse -Force
}
