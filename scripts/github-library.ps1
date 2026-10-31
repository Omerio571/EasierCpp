param(
    [ValidateSet('Discover','Resolve','Download')][string]$Action,
    [string]$Repository,
    [string]$Version = 'Latest Stable',
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$ExpectedArchiveHash = '',
    [string]$CMakeExecutable = '',
    [switch]$Repair
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
# A GUI launched from PowerShell 7 can inherit its module search path. Keep this
# Windows PowerShell worker's built-in modules discoverable without changing the
# caller's or the machine's environment.
if ($PSVersionTable.PSEdition -eq 'Desktop') {
    $env:PSModulePath = (Join-Path $PSHOME 'Modules') + ';' + $env:PSModulePath
}
# Passive discovery never configures or builds downloaded code.
if ($Repository -notmatch '^https://github\.com/([A-Za-z0-9_.-]+)/([A-Za-z0-9_.-]+?)(?:\.git)?/?$') {
    throw 'Enter a public https://github.com/owner/repository URL without credentials or query parameters.'
}
$repo = "$($matches[1])/$($matches[2])".ToLowerInvariant()
[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$utf8 = New-Object Text.UTF8Encoding($false)
function Request-GitHub([string]$Path) {
    try {
        # Invoke-RestMethod emits a JSON array as one pipeline object on Windows
        # PowerShell. Returning the captured value enumerates its release entries,
        # so draft/prerelease filtering evaluates each release rather than arrays.
        $response = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/$Path" -Headers @{
            Accept='application/vnd.github+json'; 'User-Agent'='CppLiveLab'; 'X-GitHub-Api-Version'='2022-11-28'
        } -TimeoutSec 30
        return $response
    } catch {
        $status = 0
        if ($_.Exception.Response) { $status = [int]$_.Exception.Response.StatusCode }
        if (($status -eq 404 -or $status -eq 422) -and $Path.StartsWith('commits/')) { throw "Version '$Version' was not found or could not be accessed in $repo (HTTP $status). Available alternatives are shown in the version dialog. Private repositories are unsupported." }
        if ($status -eq 404) { throw "GitHub repository/resource not found (404): $repo/$Path. Check the repository URL; private repositories are unsupported." }
        if ($status -eq 403 -or $status -eq 429) { throw "GitHub refused this request (HTTP $status, possibly API rate limit). Version existence could not be checked. Retry later." }
        throw "GitHub network/API failure (HTTP $status). Version existence could not be checked. $($_.Exception.Message)"
    }
}
if ($Action -eq 'Discover' -or ($Action -eq 'Resolve' -and $Version -eq 'Latest Stable')) {
    $releases = @(Request-GitHub 'releases?per_page=100')
    $stable = @($releases | Where-Object { -not $_.draft -and -not $_.prerelease })
    if ($Action -eq 'Discover') {
        $tags = @(Request-GitHub 'tags?per_page=100')
        $versions = @()
        if ($stable.Count) { $versions += 'Latest Stable' }
        $versions += @($releases | Where-Object { -not $_.draft } | ForEach-Object { $_.tag_name })
        $versions += @($tags | ForEach-Object { $_.name })
        $versions = @($versions | Where-Object { $_ -and $_ -notmatch '[\r\n]' } | Select-Object -Unique)
        [IO.File]::WriteAllLines((Join-Path $OutputDirectory 'versions.txt'), [string[]]$versions, $utf8)
        Write-Output "Retrieved $($versions.Count) published releases/tags (first 100 of each). A valid older tag or commit can also be entered."
        exit 0
    }
    if (-not $stable.Count) { throw 'No stable release is published. Choose an actual tag or commit instead.' }
    $Version = $stable[0].tag_name
}
if ($Action -eq 'Resolve') {
    if (-not $Version -or $Version -match '[\r\n]') { throw 'Enter a release tag or commit.' }
    $commit = Request-GitHub ('commits/' + [Uri]::EscapeDataString($Version))
    if ($commit.sha -notmatch '^[0-9a-f]{40}$') { throw 'GitHub returned an invalid commit identity.' }
    [IO.File]::WriteAllLines((Join-Path $OutputDirectory 'resolved.txt'), [string[]]@($repo, $commit.sha, $Version), $utf8)
    Write-Output "Resolved $repo $Version to exact commit $($commit.sha). No third-party code executed."
    exit 0
}
if ($Action -eq 'Download') {
    $resolved = [IO.File]::ReadAllLines((Join-Path $OutputDirectory 'resolved.txt'))
    if ($resolved[0] -ne $repo -or $resolved[1] -notmatch '^[0-9a-f]{40}$') { throw 'Resolved repository identity is invalid.' }
    $archive = Join-Path $OutputDirectory 'source.zip'
    $manifest = Join-Path $OutputDirectory 'downloaded.txt'
    $identityFile = Join-Path $OutputDirectory 'download-identity.txt'
    $identity = "$repo@$($resolved[1])"
    if ($ExpectedArchiveHash -and $ExpectedArchiveHash -notmatch '^[a-fA-F0-9]{64}$') {
        throw 'Expected archive hash is invalid.'
    }
    $reuseArchive = $false
    if ((Test-Path -LiteralPath $manifest) -and (Test-Path -LiteralPath $archive) -and
        (Test-Path -LiteralPath $identityFile) -and [IO.File]::ReadAllText($identityFile) -eq $identity) {
        $cached = [IO.File]::ReadAllLines($manifest)
        $cacheRoot = [IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\') + '\'
        if ($cached.Count -eq 2 -and [IO.Path]::GetFullPath($cached[0]).StartsWith($cacheRoot, [StringComparison]::OrdinalIgnoreCase) -and
            (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -eq $cached[1] -and
            (-not $ExpectedArchiveHash -or $cached[1] -eq $ExpectedArchiveHash)) {
            if (-not $Repair -and (Split-Path -Leaf $cached[0]) -match '^s[0-9a-f]{8}$' -and
                (Test-Path -LiteralPath $cached[0] -PathType Container)) {
                Write-Output "Reused pinned downloaded source; SHA256 $($cached[1])."
                exit 0
            }
            $reuseArchive = $true
        }
    }
    $stagedArchive = Join-Path $OutputDirectory ('download-' + [Guid]::NewGuid().ToString('N') + '.zip')
    if ($reuseArchive) {
        [IO.File]::Copy($archive, $stagedArchive)
    } else {
        Invoke-WebRequest -UseBasicParsing -Uri "https://codeload.github.com/$repo/zip/$($resolved[1])" -OutFile $stagedArchive -TimeoutSec 60
    }
    $hash = (Get-FileHash -LiteralPath $stagedArchive -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($ExpectedArchiveHash -and $hash -ne $ExpectedArchiveHash) {
        throw 'Pinned archive hash changed. Previous usable archive/source/manifest retained.'
    }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    # Failed/cancelled extraction is retained separately. Never destroy the last
    # usable source tree while repairing a cache.
    $destination = Join-Path $OutputDirectory ('source-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
    [IO.Directory]::CreateDirectory($destination) | Out-Null
    $zip = [IO.Compression.ZipFile]::OpenRead($stagedArchive)
    try {
        foreach ($entry in $zip.Entries) {
            $full = [IO.Path]::GetFullPath((Join-Path $destination $entry.FullName))
            if (-not $full.StartsWith($destination + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Archive contains an unsafe path.' }
            if (($entry.ExternalAttributes -shr 16 -band 0xF000) -eq 0xA000) { throw 'Archive symlinks are unsupported.' }
        }
    } finally { $zip.Dispose() }
    if ($CMakeExecutable) {
        if (-not [IO.File]::Exists($CMakeExecutable)) { throw 'Discovered CMake archive extractor is unavailable. Run Doctor.' }
        # Windows PowerShell's .NET Framework ZIP extractor rejects long archive
        # paths (including JSON's test reports). Use the already discovered CMake.
        Push-Location -LiteralPath $destination
        try {
            & $CMakeExecutable -E tar xf $stagedArchive
            if ($LASTEXITCODE -ne 0) { throw 'CMake archive extraction failed. Previous usable source retained.' }
        } finally { Pop-Location }
    } else {
        [IO.Compression.ZipFile]::ExtractToDirectory($stagedArchive, $destination)
    }
    $root = @(Get-ChildItem -LiteralPath $destination -Directory)
    if ($root.Count -ne 1) { throw 'Expected one GitHub archive root directory.' }
    # Ninja 1.12's Windows header checks can reject cwd + relative paths over
    # MAX_PATH even when the normalized absolute path is short. Avoid repeating
    # GitHub's full commit directory in every header dependency. Retain previous
    # source trees so existing configured projects remain usable after repair.
    $compactSource = Join-Path $OutputDirectory ('s' + [Guid]::NewGuid().ToString('N').Substring(0,8))
    $ownedRoot = [IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\') + '\'
    if (-not [IO.Path]::GetFullPath($root[0].FullName).StartsWith($ownedRoot, [StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($compactSource)) + '\' -ne $ownedRoot) {
        throw 'Compact source path escapes the selected cache directory.'
    }
    [IO.Directory]::Move($root[0].FullName, $compactSource)
    if ([IO.File]::Exists($archive)) {
        $backupArchive = Join-Path $OutputDirectory ('archive-before-' + [Guid]::NewGuid().ToString('N') + '.zip')
        [IO.File]::Replace($stagedArchive, $archive, $backupArchive)
    } else {
        [IO.File]::Move($stagedArchive, $archive)
    }
    [IO.File]::WriteAllLines((Join-Path $OutputDirectory 'downloaded.txt'), [string[]]@($compactSource, $hash), $utf8)
    [IO.File]::WriteAllText($identityFile, $identity, $utf8)
    Write-Output "Downloaded pinned source; SHA256 $hash. Download alone is not verified integration."
}
