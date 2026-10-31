param([Parameter(Mandatory=$true)][string]$Script)
$ErrorActionPreference = 'Stop'
$fixture = Join-Path $env:TEMP ('CppLiveLab GitHub fixtures ' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($fixture) | Out-Null
$global:CppLiveLabApiFixture = @{Status=0; NetworkFailure=$false; LastCommit=''; Downloads=0; DownloadFailure=$false; Archive=''}
class FixtureApiException : Exception {
    [object]$Response
    FixtureApiException([int]$status) : base('Controlled API response') {
        $this.Response = [pscustomobject]@{StatusCode=$status}
    }
}
function Invoke-RestMethod {
    param($Uri, $Headers, $TimeoutSec)
    if ($global:CppLiveLabApiFixture.Status) { throw [FixtureApiException]::new($global:CppLiveLabApiFixture.Status) }
    if ($global:CppLiveLabApiFixture.NetworkFailure) { throw 'Controlled disconnected network' }
    if ($Uri -match '/releases\?') {
        # Match Invoke-RestMethod's actual Windows PowerShell array output.
        Write-Output -NoEnumerate @(
            [pscustomobject]@{tag_name='v4.0-draft'; draft=$true; prerelease=$false},
            [pscustomobject]@{tag_name='v3.0-beta'; draft=$false; prerelease=$true},
            [pscustomobject]@{tag_name='v2.0'; draft=$false; prerelease=$false},
            [pscustomobject]@{tag_name='v1.0'; draft=$false; prerelease=$false})
        return
    }
    if ($Uri -match '/tags\?') { Write-Output -NoEnumerate @([pscustomobject]@{name='v0.5'}); return }
    if ($Uri -match '/commits/(.*)$') { $global:CppLiveLabApiFixture.LastCommit=$matches[1]; return [pscustomobject]@{sha=('a' * 40)} }
    throw "Unexpected fixture request $Uri"
}
function Require([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Invoke-WebRequest {
    param([switch]$UseBasicParsing, $Uri, $OutFile, $TimeoutSec)
    $global:CppLiveLabApiFixture.Downloads++
    if ($global:CppLiveLabApiFixture.DownloadFailure) { throw 'Controlled download failure' }
    [IO.File]::Copy($global:CppLiveLabApiFixture.Archive, $OutFile, $true)
}
function Refuses([scriptblock]$Action, [string]$Pattern) {
    $message = ''
    try { & $Action } catch { $message=$_.Exception.Message }
    Require ($message -match $Pattern) "Expected '$Pattern', got '$message'"
}
try {
    & $Script -Action Discover -Repository 'https://github.com/example/library' -OutputDirectory $fixture
    $versions = [IO.File]::ReadAllLines((Join-Path $fixture 'versions.txt'))
    Require ($versions[0] -eq 'Latest Stable' -and $versions -contains 'v0.5' -and $versions -contains 'v3.0-beta') 'Actual release/tag alternatives missing'
    Require ($versions -notcontains 'v4.0-draft') 'Draft release was offered as a published version'
    & $Script -Action Resolve -Repository 'https://github.com/example/library' -Version 'Latest Stable' -OutputDirectory $fixture
    Require ($global:CppLiveLabApiFixture.LastCommit -eq 'v2.0') 'Latest Stable incorrectly selected prerelease'
    $resolved = [IO.File]::ReadAllLines((Join-Path $fixture 'resolved.txt'))
    Require ($resolved[1] -eq ('a'*40) -and $resolved[2] -eq 'v2.0') 'Exact reproducible version was not saved'
    & $Script -Action Resolve -Repository 'https://github.com/example/library' -Version 'v0.5' -OutputDirectory $fixture
    Require ($global:CppLiveLabApiFixture.LastCommit -eq 'v0.5') 'Older explicit tag was replaced'
    Refuses { & $Script -Action Discover -Repository 'https://github.com/user/repo?token=private' -OutputDirectory $fixture } 'without credentials'
    foreach ($status in @(403,429)) {
        $global:CppLiveLabApiFixture.Status=$status
        Refuses { & $Script -Action Resolve -Repository 'https://github.com/example/library' -Version 'v9' -OutputDirectory $fixture } 'existence could not be checked'
    }
    $global:CppLiveLabApiFixture.Status=404
    Refuses { & $Script -Action Resolve -Repository 'https://github.com/example/library' -Version 'v9' -OutputDirectory $fixture } "Version 'v9'"
    $global:CppLiveLabApiFixture.Status=0; $global:CppLiveLabApiFixture.NetworkFailure=$true
    Refuses { & $Script -Action Resolve -Repository 'https://github.com/example/library' -Version 'v9' -OutputDirectory $fixture } 'network/API failure'
    $global:CppLiveLabApiFixture.NetworkFailure=$false
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archiveSource=Join-Path $fixture 'archive-content'
    [IO.Directory]::CreateDirectory((Join-Path $archiveSource 'fixture-revision')) | Out-Null
    [IO.File]::WriteAllText((Join-Path $archiveSource 'fixture-revision\header.hpp'), '// pinned fixture')
    $global:CppLiveLabApiFixture.Archive=Join-Path $fixture 'fixture.zip'
    [IO.Compression.ZipFile]::CreateFromDirectory($archiveSource, $global:CppLiveLabApiFixture.Archive)
    & $Script -Action Resolve -Repository 'https://github.com/example/library' -Version 'v1.0' -OutputDirectory $fixture
    & $Script -Action Download -Repository 'https://github.com/example/library' -OutputDirectory $fixture
    $usable=[IO.File]::ReadAllLines((Join-Path $fixture 'downloaded.txt'))
    Require ((Split-Path -Leaf $usable[0]) -match '^s[0-9a-f]{8}$' -and
        (Split-Path -Parent $usable[0]) -eq $fixture) 'Extracted source root was not compact and contained in its cache'
    & $Script -Action Download -Repository 'https://github.com/example/library' -OutputDirectory $fixture
    Require ($global:CppLiveLabApiFixture.Downloads -eq 1) 'Unchanged pin was downloaded unnecessarily'
    $legacySource=Join-Path $fixture 'legacy-github-full-commit-source'
    [IO.Directory]::CreateDirectory($legacySource) | Out-Null
    [IO.File]::WriteAllText((Join-Path $legacySource 'header.hpp'), '// pinned fixture')
    [IO.File]::WriteAllLines((Join-Path $fixture 'downloaded.txt'), [string[]]@($legacySource,$usable[1]))
    & $Script -Action Download -Repository 'https://github.com/example/library' -OutputDirectory $fixture -ExpectedArchiveHash $usable[1]
    $usable=[IO.File]::ReadAllLines((Join-Path $fixture 'downloaded.txt'))
    Require ($global:CppLiveLabApiFixture.Downloads -eq 1 -and
        (Split-Path -Leaf $usable[0]) -match '^s[0-9a-f]{8}$' -and
        [IO.File]::ReadAllText((Join-Path $legacySource 'header.hpp')) -eq '// pinned fixture') 'Legacy cache compaction downloaded again or altered the previous source tree'
    [IO.File]::WriteAllText((Join-Path $usable[0] 'header.hpp'), '// corrupted source')
    & $Script -Action Download -Repository 'https://github.com/example/library' -OutputDirectory $fixture -Repair -ExpectedArchiveHash $usable[1]
    $usable=[IO.File]::ReadAllLines((Join-Path $fixture 'downloaded.txt'))
    Require ($global:CppLiveLabApiFixture.Downloads -eq 1 -and
        [IO.File]::ReadAllText((Join-Path $usable[0] 'header.hpp')) -eq '// pinned fixture') 'Repair did not re-extract the verified archive without redownloading'
    $archiveHash=(Get-FileHash -LiteralPath (Join-Path $fixture 'source.zip') -Algorithm SHA256).Hash
    Refuses { & $Script -Action Download -Repository 'https://github.com/example/library' -OutputDirectory $fixture -ExpectedArchiveHash ('0'*64) } 'Pinned archive hash changed'
    Require ((Get-FileHash -LiteralPath (Join-Path $fixture 'source.zip') -Algorithm SHA256).Hash -eq $archiveHash) 'Hash mismatch replaced the previous usable archive'
    $global:CppLiveLabApiFixture.DownloadFailure=$true
    [IO.File]::WriteAllText((Join-Path $fixture 'download-identity.txt'), 'different identity')
    Refuses { & $Script -Action Download -Repository 'https://github.com/example/library' -OutputDirectory $fixture } 'Controlled download failure'
    Require ([IO.File]::ReadAllText((Join-Path $usable[0] 'header.hpp')) -eq '// pinned fixture') 'Failed download deleted the previous usable source'
    Require ([IO.File]::ReadAllLines((Join-Path $fixture 'downloaded.txt'))[0] -eq $usable[0]) 'Failed download published a replacement manifest'
    $global:CppLiveLabApiFixture.DownloadFailure=$false
    $longArchive=Join-Path $fixture 'long-path.zip'
    $longEntry='fixture-revision/include/' + ('a'*100) + '/' + ('b'*100) + '/value.hpp'
    $zip=[IO.Compression.ZipFile]::Open($longArchive, [IO.Compression.ZipArchiveMode]::Create)
    try {
        $entry=$zip.CreateEntry($longEntry)
        $writer=New-Object IO.StreamWriter($entry.Open())
        try { $writer.Write('// long path fixture') } finally { $writer.Dispose() }
    } finally { $zip.Dispose() }
    $global:CppLiveLabApiFixture.Archive=$longArchive
    $cmake=(Get-Command cmake.exe -ErrorAction Stop).Source
    & $Script -Action Download -Repository 'https://github.com/example/library' -OutputDirectory $fixture -CMakeExecutable $cmake
    $longSource=[IO.File]::ReadAllLines((Join-Path $fixture 'downloaded.txt'))[0]
    $expected=Join-Path $fixture 'expected.hpp'
    [IO.File]::WriteAllText($expected, '// long path fixture')
    & $cmake -E compare_files $expected ($longSource + '\include\' + ('a'*100) + '\' + ('b'*100) + '\value.hpp')
    Require ($LASTEXITCODE -eq 0) 'Discovered CMake did not extract the real long archive path correctly'
    Write-Host 'GitHub offline fixtures passed: releases, tags, prerelease exclusion, exact pin, older release, URL validation, missing version, rate limit, disconnected network.'
} finally {
    # This exclusively created disposable fixture is the only removal target.
    $resolvedFixture=[IO.Path]::GetFullPath($fixture)
    if ([IO.Path]::GetDirectoryName($resolvedFixture) -ne [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') -or
        [IO.Path]::GetFileName($resolvedFixture) -notlike 'CppLiveLab GitHub fixtures *') {
        throw 'Fixture cleanup escaped its exclusively created temporary directory.'
    }
    Remove-Item -LiteralPath ('\\?\' + $resolvedFixture) -Recurse -Force
    Remove-Variable -Name CppLiveLabApiFixture -Scope Global
}
