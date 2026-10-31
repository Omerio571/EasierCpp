param(
    [switch]$Apply
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Get-Location).Path
$includeRoot = Join-Path $repositoryRoot 'include'

if (-not (Test-Path -LiteralPath (Join-Path $repositoryRoot 'CMakeLists.txt'))) {
    throw 'Run this script from the EasierCpp repository root.'
}

# Preserve include/cpplivelab/*.hpp: these are existing public API paths.
# Only relocate currently flat headers directly under include/.
$groups = [ordered]@{
    appearance = @('appearance.hpp', 'ui_theme.hpp')
    build = @('auto_build.hpp', 'build_diagnostics.hpp', 'build_freshness.hpp', 'build_lease.hpp')
    cli = @('workspace_cli.hpp')
    gui = @('console_view.hpp', 'project_dialogs.hpp', 'library_manager.hpp', 'library_workflow.hpp')
    libraries = @('library_compatibility.hpp')
    process = @('application_windows.hpp', 'child_startup.hpp', 'console_model.hpp', 'output_decoder.hpp', 'restart.hpp', 'windows_support.hpp')
    projects = @('directory_scan.hpp', 'project_paths.hpp', 'project_templates.hpp')
    tools = @('tool_workflows.hpp')
}

$headerDestinations = @{}
foreach ($group in $groups.Keys) {
    foreach ($fileName in $groups[$group]) {
        if ($headerDestinations.ContainsKey($fileName)) {
            throw "Duplicate header mapping: $fileName"
        }
        $headerDestinations[$fileName] = "$group/$fileName"
    }
}

# Check the mapping and collisions before changing anything.
foreach ($fileName in $headerDestinations.Keys) {
    $oldPath = Join-Path $includeRoot $fileName
    $newPath = Join-Path $includeRoot ($headerDestinations[$fileName].Replace('/', '\'))
    $oldExists = Test-Path -LiteralPath $oldPath -PathType Leaf
    $newExists = Test-Path -LiteralPath $newPath -PathType Leaf

    if ($oldExists -and $newExists) {
        throw "Both source and destination exist for $fileName. Resolve manually."
    }
    if (-not $oldExists -and -not $newExists) {
        throw "Header not found at either expected location: $fileName"
    }
}

# Transform only quoted includes that refer to a mapped, previously flat header.
# Includes in the public cpplivelab/ namespace are intentionally left alone.
$includePattern = '(?m)^(?<prefix>[ \t]*#[ \t]*include[ \t]*")(?<path>[^"\r\n]+)(?<suffix>"[^\r\n]*)$'
$sourceFiles = Get-ChildItem -Path @(
    (Join-Path $repositoryRoot 'src'),
    (Join-Path $repositoryRoot 'include'),
    (Join-Path $repositoryRoot 'tests')
) -Recurse -File | Where-Object { $_.Extension -in @('.cpp', '.hpp', '.h', '.cc', '.cxx') }

$changedFiles = 0
foreach ($file in $sourceFiles) {
    $original = [IO.File]::ReadAllText($file.FullName)
    $updated = [regex]::Replace($original, $includePattern, [System.Text.RegularExpressions.MatchEvaluator]{
        param($match)
        $oldInclude = $match.Groups['path'].Value.Replace('\', '/')
        $fileName = ($oldInclude -split '/')[-1]

        if (-not $headerDestinations.ContainsKey($fileName)) {
            return $match.Value
        }

        $eligible = $oldInclude -eq $fileName -or
            $oldInclude -eq "include/$fileName" -or
            $oldInclude -match "^(?:\.\./)+(?:src|include)/$([regex]::Escape($fileName))$"

        if (-not $eligible) {
            return $match.Value
        }

        return $match.Groups['prefix'].Value +
            $headerDestinations[$fileName] +
            $match.Groups['suffix'].Value
    })

    if ($updated -ne $original) {
        $changedFiles++
        $relative = $file.FullName.Substring($repositoryRoot.Length + 1)
        Write-Host "Update includes: $relative"
        if ($Apply) {
            # Keep existing UTF-8 BOM where present; otherwise write UTF-8 without BOM.
            $bytes = [IO.File]::ReadAllBytes($file.FullName)
            $withBom = $bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and
                $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF
            $encoding = [System.Text.UTF8Encoding]::new($withBom)
            [IO.File]::WriteAllText($file.FullName, $updated, $encoding)
        }
    }
}

foreach ($group in $groups.Keys) {
    foreach ($fileName in $groups[$group]) {
        $oldPath = Join-Path $includeRoot $fileName
        $newPath = Join-Path $includeRoot ($headerDestinations[$fileName].Replace('/', '\'))
        if (Test-Path -LiteralPath $oldPath -PathType Leaf) {
            Write-Host "Move: include/$fileName -> include/$($headerDestinations[$fileName])"
            if ($Apply) {
                New-Item -ItemType Directory -Path (Split-Path $newPath) -Force | Out-Null
                Move-Item -LiteralPath $oldPath -Destination $newPath
            }
        }
    }
}

if ($Apply) {
    Write-Host "Header organization applied; includes updated in $changedFiles files."
    Write-Warning 'Check remaining relative paths, scripts, and generated templates before rebuilding.'
} else {
    Write-Host "PREVIEW ONLY: $changedFiles files need include updates. Re-run with -Apply to modify files."
}
