param(
    [Parameter(Mandatory=$true)][string]$Project,
    [string]$Build,
    [string]$Configuration = 'Debug',
    [string]$Target,
    [Parameter(Mandatory=$true)][string]$Output
)
$ErrorActionPreference = 'Stop'
$Project = [IO.Path]::GetFullPath($Project).TrimEnd('\', '/')
$rows = [Collections.Generic.List[string]]::new()
$seen = @{}
$script:metadataBytes = 0

# This reader never invokes CMake or interprets project code. Output is a bounded,
# eight-line UTF-8 record stream consumed by the existing background UI operation.
function Add-Library($identity, $name, $source, $ownership, $evidence, $targetName = '', $confidence = 'Declaration') {
    if ($seen.ContainsKey($identity) -or $seen.Count -ge 2048) { return }
    $seen[$identity] = $true
    foreach ($field in @($identity, $name, $source, 'Unknown', $ownership, $evidence, $targetName, $confidence)) {
        $rows.Add(([string]$field -replace '[\r\n\x00-\x1f]', ' '))
    }
}

function Read-Json($path) {
    $file = Get-Item -LiteralPath $path
    $script:metadataBytes += $file.Length
    if ($file.Length -gt 8MB -or $script:metadataBytes -gt 32MB) { throw 'Dependency metadata exceeds the passive reader limit.' }
    return ([IO.File]::ReadAllText($file.FullName) | ConvertFrom-Json)
}

function Read-Reply($name) {
    if ($name -notmatch '^[A-Za-z0-9_.-]+\.json$') { throw 'Invalid File API reply filename.' }
    return Read-Json (Join-Path $reply $name)
}

function Same-Path($left, $right) {
    return [IO.Path]::GetFullPath($left).TrimEnd('\', '/') -eq [IO.Path]::GetFullPath($right).TrimEnd('\', '/')
}

$configured = $false
if ($Build -and (Test-Path -LiteralPath (Join-Path $Build '.cmake/api/v1/reply'))) {
    try {
        $reply = Join-Path $Build '.cmake/api/v1/reply'
        $indexFile = Get-ChildItem -LiteralPath $reply -Filter 'index-*.json' |
            Sort-Object Name -Descending | Select-Object -First 1
        if ($indexFile) {
            $index = Read-Json $indexFile.FullName
            $modelEntry = @($index.objects | Where-Object { $_.kind -eq 'codemodel' -and $_.version.major -eq 2 }) | Select-Object -First 1
            if ($modelEntry) {
                $model = Read-Reply $modelEntry.jsonFile
                if (-not (Same-Path $model.paths.source $Project)) { throw 'File API belongs to another project.' }
                if (-not (Same-Path $model.paths.build $Build)) { throw 'File API belongs to another build directory.' }
                $stale = $false
                $inputs = @((Join-Path $Project 'CMakeLists.txt'), (Join-Path $Build 'CMakeCache.txt'))
                $filesEntry = @($index.objects | Where-Object { $_.kind -eq 'cmakeFiles' }) | Select-Object -First 1
                if ($filesEntry) {
                    $cmakeFiles = Read-Reply $filesEntry.jsonFile
                    foreach ($inputFile in $cmakeFiles.inputs) {
                        if ($inputFile.isCMake -or $inputFile.isGenerated) { continue }
                        $path = $inputFile.path
                        if (-not [IO.Path]::IsPathRooted($path)) { $path = Join-Path $Project $path }
                        $inputs += $path
                    }
                }
                $config = @($model.configurations | Where-Object { $_.name -eq $Configuration -or $_.name -eq '' }) | Select-Object -First 1
                if ($config) {
                    foreach ($directory in $config.directories) {
                        $path = $directory.source
                        if (-not [IO.Path]::IsPathRooted($path)) { $path = Join-Path $Project $path }
                        $inputs += Join-Path $path 'CMakeLists.txt'
                    }
                    foreach ($inputPath in $inputs) {
                        if (-not (Test-Path -LiteralPath $inputPath) -or
                            [IO.File]::GetLastWriteTimeUtc($inputPath) -gt $indexFile.LastWriteTimeUtc) { $stale = $true }
                    }
                    $targets = @{}
                    if (@($config.targets).Count + @($config.abstractTargets).Count -gt 4096) {
                        throw 'Too many targets for bounded passive inspection.'
                    }
                    foreach ($entry in @($config.targets) + @($config.abstractTargets)) {
                        if ($entry.jsonFile) { $targets[$entry.id] = Read-Reply $entry.jsonFile }
                    }
                    $pending = [Collections.Generic.Queue[string]]::new()
                    foreach ($id in $targets.Keys) {
                        if (($Target -and $targets[$id].name -eq $Target) -or
                            (-not $Target -and $targets[$id].type -eq 'EXECUTABLE')) { $pending.Enqueue($id) }
                    }
                    $visited = @{}
                    while ($pending.Count) {
                        $id = $pending.Dequeue()
                        if ($visited.ContainsKey($id) -or -not $targets.ContainsKey($id)) { continue }
                        $visited[$id] = $true
                        $detail = $targets[$id]
                        foreach ($dependency in $detail.dependencies) { $pending.Enqueue($dependency.id) }
                        if ($detail.type -match '^(STATIC|SHARED|MODULE|INTERFACE|UNKNOWN)_LIBRARY$') {
                            $confidence = if ($stale) { 'StaleSnapshot' } else { 'ConfiguredSnapshot' }
                            $age = if ($stale) { 'stale; configuration inputs changed' } else { 'existing configured snapshot; not a build/compatibility result' }
                            Add-Library ('target:' + $id) $detail.name 'CMake File API' 'External' (
                                "$($detail.type), $Configuration; $age. Freshness of arbitrary included scripts is not guaranteed. Source: $($detail.paths.source). Imported: $([bool]$detail.imported). Snapshot: $($indexFile.Name)") $detail.name $confidence
                        }
                    }
                    $configured = -not $stale -and $visited.Count -gt 0
                }
            }
        }
    } catch {
        Add-Library 'metadata-error' 'Dependency metadata unavailable' 'CMake File API' 'Unverified' 'Existing metadata is malformed, mismatched, missing or exceeds passive reader limits. No configure operation ran.' '' 'Unavailable'
    }
}

# Literal declarations are hints only, never an evaluation of conditionals,
# variables, macros, package ownership, or arbitrary included CMake scripts.
# Bound the scan to the root and its small conventional cmake directory.
& {
    $files = @((Join-Path $Project 'CMakeLists.txt'))
    $modules = Join-Path $Project 'cmake'
    if (Test-Path -LiteralPath $modules) {
        $files += @(Get-ChildItem -LiteralPath $modules -File -Filter '*.cmake' | Select-Object -First 64 | ForEach-Object FullName)
    }
    foreach ($file in $files) {
        if (-not (Test-Path -LiteralPath $file) -or (Get-Item -LiteralPath $file).Length -gt 1MB) { continue }
        $text = [IO.File]::ReadAllText($file)
        $text = [regex]::Replace($text, '(?s)#\[(=*)\[.*?\]\1\]', '')
        $text = [regex]::Replace($text, '(?m)#[^\r\n]*', '')
        foreach ($match in [regex]::Matches($text, '(?i)\b(find_package|FetchContent_Declare|add_subdirectory)\s*\(\s*"?([A-Za-z0-9_./+-]+)(?=[\s")])')) {
            $method = $match.Groups[1].Value.ToLowerInvariant()
            if ($configured -and $method -ne 'find_package') { continue }
            $name = $match.Groups[2].Value
            Add-Library ($method + ':' + $name) $name ('CMake / ' + $method) 'Unverified' (
                "Literal declaration in $file. Not evaluated; may be conditional, unused, or unavailable. Version unknown.")
        }
    }
}

$manifest = Join-Path $Project 'vcpkg.json'
if (Test-Path -LiteralPath $manifest) {
    try {
        $vcpkg = Read-Json $manifest
        foreach ($dependency in $vcpkg.dependencies) {
            $name = if ($dependency -is [string]) { $dependency } else { $dependency.name }
            if ($name -match '^[a-z0-9]+(?:-[a-z0-9]+)*$') {
                Add-Library ('vcpkg:' + $name) $name 'vcpkg manifest' 'Unverified' (
                    'Declared in vcpkg.json; installation, active features/platform and resolved version are not verified.')
            }
        }
    } catch {
        Add-Library 'vcpkg-error' 'vcpkg manifest unreadable' 'vcpkg manifest' 'Unverified' 'The manifest could not be safely parsed; no package operation ran.' '' 'Unavailable'
    }
}
[IO.File]::WriteAllLines($Output, $rows, [Text.UTF8Encoding]::new($false))
