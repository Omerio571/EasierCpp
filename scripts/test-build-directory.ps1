# First-party cache validation shared with the focused migration tests.
# Invalid caches are retained. Recovery selects a separate manager build tree.
function Resolve-EasierCppTestBuild {
    param([string]$Repository, [string]$Directory)
    $repositoryPath = [IO.Path]::GetFullPath($Repository).TrimEnd('\', '/')
    $directoryPath = [IO.Path]::GetFullPath($Directory).TrimEnd('\', '/')
    function Read-CacheValue($text, $name) {
        $match = [regex]::Match($text, '(?m)^' + [regex]::Escape($name) + ':[^=]+=(.*)\r?$')
        if ($match.Success) { return $match.Groups[1].Value.Trim() }
        return ''
    }
    function Cache-Matches($path) {
        $cache = Join-Path $path 'CMakeCache.txt'
        if (-not (Test-Path -LiteralPath $cache -PathType Leaf)) { return $false }
        $text = [IO.File]::ReadAllText($cache)
        $recordedSource = Read-CacheValue $text 'CMAKE_HOME_DIRECTORY'
        $recordedBuild = Read-CacheValue $text 'CMAKE_CACHEFILE_DIR'
        return $recordedSource -and $recordedBuild -and
            [IO.Path]::GetFullPath($recordedSource).TrimEnd('\','/') -eq $repositoryPath -and
            [IO.Path]::GetFullPath($recordedBuild).TrimEnd('\','/') -eq $path -and
            (Read-CacheValue $text 'CMAKE_BUILD_TYPE') -eq 'Debug' -and
            (Read-CacheValue $text 'CMAKE_GENERATOR') -eq 'Ninja' -and
            (Read-CacheValue $text 'CMAKE_PROJECT_NAME') -in 'CppLiveLab','EasierCpp'
    }
    $cache = Join-Path $directoryPath 'CMakeCache.txt'
    $ownedRoot = Join-Path $repositoryPath 'build'
    if ($directoryPath.StartsWith($ownedRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
        $current = $directoryPath
        while ($current.Length -ge $ownedRoot.Length) {
            if ((Test-Path -LiteralPath $current) -and
                ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
                throw "Linked test build directory refused: $current. Choose a fresh local build directory."
            }
            $current = Split-Path $current -Parent
        }
        if ((Test-Path -LiteralPath $cache) -and
            ((Get-Item -LiteralPath $cache -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Linked CMake cache refused: $cache. No cache was changed."
        }
    }
    if (-not (Test-Path -LiteralPath $cache) -or (Cache-Matches $directoryPath)) {
        return $directoryPath
    }
    $text = [IO.File]::ReadAllText($cache)
    if (-not $directoryPath.StartsWith($ownedRoot + '\', [StringComparison]::OrdinalIgnoreCase) -or
        (Read-CacheValue $text 'CMAKE_PROJECT_NAME') -notin 'CppLiveLab','EasierCpp') {
        throw "Unverified or foreign cache retained at $cache. Choose a fresh directory under $ownedRoot; no cache was removed."
    }
    for ($index = 1; $index -le 20; $index++) {
        $candidate = $directoryPath + ".easiercpp-$index"
        $candidateCache = Join-Path $candidate 'CMakeCache.txt'
        if (((Test-Path -LiteralPath $candidate) -and
                ((Get-Item -LiteralPath $candidate -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) -or
            ((Test-Path -LiteralPath $candidateCache) -and
                ((Get-Item -LiteralPath $candidateCache -Force).Attributes -band [IO.FileAttributes]::ReparsePoint))) {
            continue
        }
        if (-not (Test-Path -LiteralPath $candidate) -or (Cache-Matches $candidate)) {
            Write-Host "[EasierCpp] Stale/moved test cache preserved at $directoryPath. Using $candidate."
            return $candidate
        }
    }
    throw "No verified recovery directory is available. Choose a fresh BuildDirectory; $cache was preserved."
}
