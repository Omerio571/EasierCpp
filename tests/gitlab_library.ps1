param([Parameter(Mandatory=$true)][string]$Script)
$ErrorActionPreference = 'Stop'
$root = Join-Path $env:TEMP ('EasierCPP-GitLab-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
$global:requests = @()
$global:status = 0
$global:noTags = $false
$global:downloadUrl = ''

class GitLabFixtureException : Exception {
    [object]$Response
    GitLabFixtureException([int]$status) : base('Controlled GitLab error') {
        $this.Response = [pscustomobject]@{ StatusCode = $status }
    }
}

function Invoke-RestMethod {
    param($Uri, $Headers, $TimeoutSec)
    $global:requests += $Uri
    if ($global:status) { throw [GitLabFixtureException]::new($global:status) }
    if ($Uri -notlike 'https://gitlab.com/api/v4/projects/group%2Fsub%2Flibrary/*') {
        throw "Unexpected project endpoint: $Uri"
    }
    if ($Uri -match '/releases\?') {
        if ($global:noTags) { return @() }
        return @(
            [pscustomobject]@{tag_name='v3.0-rc1'; released_at='2020-01-01T00:00:00Z'},
            [pscustomobject]@{tag_name='v2.0'; released_at='2020-01-01T00:00:00Z'}
        )
    }
    if ($Uri -match '/repository/tags\?') {
        if ($global:noTags) { return @() }
        return @([pscustomobject]@{name='v1.0'})
    }
    if ($Uri -match '/repository/commits/') {
        return [pscustomobject]@{id=('a' * 40)}
    }
    throw "Unexpected endpoint: $Uri"
}

function Invoke-WebRequest {
    param([switch]$UseBasicParsing, $Uri, $OutFile, $TimeoutSec)
    $global:downloadUrl = $Uri
    [IO.File]::Copy((Join-Path $root 'fixture.zip'), $OutFile)
}

function Require([bool]$condition, [string]$message) {
    if (-not $condition) { throw $message }
}

function Refuses([scriptblock]$action, [string]$pattern) {
    $message = ''
    try { & $action } catch { $message = $_.Exception.Message }
    Require ($message -match $pattern) "Expected '$pattern', got '$message'"
}

try {
    $repo = 'https://gitlab.com/group/sub/library.git'
    $out = Join-Path $root 'work'
    & $Script -Action Discover -Repository $repo -OutputDirectory $out
    $versions = [IO.File]::ReadAllLines((Join-Path $out 'versions.txt'))
    Require ($versions[0] -eq 'Latest Stable' -and $versions -contains 'v1.0') 'Tag discovery failed'
    $requestCount = $global:requests.Count
    & $Script -Action Discover -Repository $repo -OutputDirectory $out
    Require ($global:requests.Count -eq $requestCount) 'Metadata cache was not reused'
    & $Script -Action Resolve -Repository $repo -OutputDirectory $out
    $resolved = [IO.File]::ReadAllLines((Join-Path $out 'resolved.txt'))
    Require ($resolved[0] -eq 'gitlab.com/group/sub/library' -and $resolved[2] -eq 'v2.0') 'Stable release pin failed'
    & $Script -Action Resolve -Repository 'gitlab.com/group/sub/library' -Version 'feature/test' -OutputDirectory $out
    Require ($global:requests[-1] -match 'commits/feature%2Ftest$') 'Branch was not escaped'

    foreach ($invalid in @('https://gitlab.com/a/../b', 'https://gitlab.com/a/-/tree/main',
        'https://user@gitlab.com/a/b', 'https://gitlab.com/a/b?token=secret',
        'https://gitlab.example.com/a/b', 'https://gitlab.com/a')) {
        Refuses { & $Script -Action Resolve -Repository $invalid -Version main -OutputDirectory $out } 'Invalid|Enter a public'
    }
    foreach ($code in @(401, 403, 404, 429)) {
        $global:status = $code
        Refuses { & $Script -Action Resolve -Repository $repo -Version main -OutputDirectory $out } "$code"
    }
    $global:status = 0
    $global:noTags = $true
    & $Script -Action Discover -Repository $repo -OutputDirectory (Join-Path $root 'empty')
    Require (([IO.File]::ReadAllLines((Join-Path $root 'empty/versions.txt'))).Count -eq 0) 'Tagless repository fabricated versions'
    Refuses { & $Script -Action Resolve -Repository $repo -OutputDirectory $out } 'No stable release'
    & $Script -Action Resolve -Repository $repo -Version main -OutputDirectory $out

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $source = Join-Path $root 'archive/library'
    [IO.Directory]::CreateDirectory($source) | Out-Null
    [IO.File]::WriteAllText((Join-Path $source 'CMakeLists.txt'), 'controlled fixture; never executed')
    [IO.Compression.ZipFile]::CreateFromDirectory((Join-Path $root 'archive'), (Join-Path $root 'fixture.zip'))
    & $Script -Action Download -Repository $repo -OutputDirectory $out
    $downloaded = [IO.File]::ReadAllLines((Join-Path $out 'downloaded.txt'))
    Require ($global:downloadUrl -eq ('https://gitlab.com/api/v4/projects/group%2Fsub%2Flibrary/repository/archive.zip?sha=' + ('a' * 40))) 'Incorrect pinned archive URL'
    Require (Test-Path -LiteralPath (Join-Path $downloaded[0] 'CMakeLists.txt')) 'Archive extraction failed'
    Require ($downloaded[1] -eq (Get-FileHash (Join-Path $root 'fixture.zip')).Hash.ToLowerInvariant()) 'Incorrect archive hash'
    & $Script -Action Download -Repository $repo -OutputDirectory $out -ExpectedArchiveHash $downloaded[1]
    Refuses { & $Script -Action Download -Repository $repo -OutputDirectory $out -ExpectedArchiveHash ('b' * 64) } 'hash changed'
    Write-Output 'GitLab controlled API/archive fixtures passed; no network or third-party code execution.'
} finally {
    $resolvedRoot = [IO.Path]::GetFullPath($root)
    $expectedParent = [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\')
    if ([IO.Path]::GetDirectoryName($resolvedRoot) -ne $expectedParent -or
        [IO.Path]::GetFileName($resolvedRoot) -notmatch '^EasierCPP-GitLab-[0-9a-f]{32}$') {
        throw 'Refusing cleanup outside the owned temporary GitLab fixture.'
    }
    Remove-Item -LiteralPath $resolvedRoot -Recurse -Force
}
