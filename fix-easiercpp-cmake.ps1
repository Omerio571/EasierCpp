param(
    [switch]$Apply
)

$ErrorActionPreference = 'Stop'
$root = (Get-Location).Path
$rootCmake = Join-Path $root 'CMakeLists.txt'

if (-not (Test-Path -LiteralPath $rootCmake -PathType Leaf)) {
    throw 'Run this script from the EasierCpp repository root.'
}

# Paths established by the source, header, and test organization passes.
# The file system is authoritative: missing moves are not invented.
$sourceGroups = [ordered]@{
    appearance = @('appearance.cpp', 'ui_theme.cpp')
    build = @('auto_build.cpp', 'build_freshness.cpp')
    cli = @('workspace_cli.cpp', 'doctor.cpp')
    gui = @('gui.cpp', 'console_view.cpp', 'project_dialogs.cpp', 'library_manager.cpp', 'library_workflow.cpp')
    libraries = @('libraries.cpp', 'library_compatibility.cpp')
    process = @('process.cpp', 'application_windows.cpp', 'restart.cpp', 'session.cpp', 'console_model.cpp', 'output_decoder.cpp')
    projects = @('projects.cpp', 'workspace.cpp', 'workspace_creation.cpp')
    tools = @('tool_workflows.cpp', 'editors.cpp')
}

$testGroups = [ordered]@{
    build = @('stage4_tests.cpp', 'performance_probe.cpp')
    cli = @('doctor_tests.cpp')
    gui = @('console_view_tests.cpp', 'gui_flow_tests.cpp', 'restart_gui_tests.cpp', 'ui_audit_tests.cpp', 'ui_forms_tests.cpp')
    libraries = @('library_compatibility_tests.cpp', 'library_live_tests.cpp', 'library_management_tests.cpp')
    process = @('redirection_tests.cpp', 'reliability_tests.cpp', 'restart_tests.cpp', 'session_fixture.cpp', 'session_tests.cpp')
    projects = @('stage5_tests.cpp', 'unified_tests.cpp')
    maintenance = @('maintenance_tests.cpp')
    tools = @('workflow_probe.cpp')
}

$headerGroups = [ordered]@{
    appearance = @('appearance.hpp', 'ui_theme.hpp')
    build = @('auto_build.hpp', 'build_diagnostics.hpp', 'build_freshness.hpp', 'build_lease.hpp')
    cli = @('workspace_cli.hpp')
    gui = @('console_view.hpp', 'project_dialogs.hpp', 'library_manager.hpp', 'library_workflow.hpp')
    libraries = @('library_compatibility.hpp')
    process = @('application_windows.hpp', 'child_startup.hpp', 'console_model.hpp', 'output_decoder.hpp', 'restart.hpp', 'windows_support.hpp')
    projects = @('directory_scan.hpp', 'project_paths.hpp', 'project_templates.hpp')
    tools = @('tool_workflows.hpp')
}

$pathMap = [ordered]@{}
foreach ($group in $sourceGroups.Keys) {
    foreach ($name in $sourceGroups[$group]) {
        $pathMap["src/$name"] = "src/$group/$name"
    }
}
foreach ($group in $testGroups.Keys) {
    foreach ($name in $testGroups[$group]) {
        $pathMap["tests/$name"] = "tests/$group/$name"
    }
}

function Get-Absolute([string]$relativePath) {
    Join-Path $root $relativePath.Replace('/', [IO.Path]::DirectorySeparatorChar)
}

# Before writing anything, check that every mapping is unambiguous.
foreach ($oldPath in $pathMap.Keys) {
    $newPath = $pathMap[$oldPath]
    $oldExists = Test-Path -LiteralPath (Get-Absolute $oldPath) -PathType Leaf
    $newExists = Test-Path -LiteralPath (Get-Absolute $newPath) -PathType Leaf

    if ($oldExists -and $newExists) {
        throw "Duplicate source at both locations: $oldPath and $newPath"
    }
    if (-not $oldExists -and -not $newExists) {
        throw "Missing source at both locations: $oldPath and $newPath"
    }
}

# Update only literal source paths; keep all target names, flags, and test labels.
# Exclude generated and third-party build trees.
$cmakeFiles = @(Get-ChildItem -LiteralPath $root -File -Recurse |
    Where-Object {
        ($_.Name -eq 'CMakeLists.txt' -or $_.Extension -eq '.cmake') -and
        $_.FullName -notmatch '[\\/](build|out|\.git|\.vs|vendor|third_party|CMakeFiles)[\\/]'
    })

$edits = @{}
foreach ($file in $cmakeFiles) {
    $original = [IO.File]::ReadAllText($file.FullName)
    $updated = $original

    foreach ($oldPath in $pathMap.Keys) {
        $newPath = $pathMap[$oldPath]
        # A bare path in a CMake list, quoted path, or command argument.
        # Guard against re-replacing already organized folder paths.
        $pattern = '(?<![A-Za-z0-9_./\\-])' + [regex]::Escape($oldPath) + '(?![A-Za-z0-9_./\\-])'
        $updated = [regex]::Replace($updated, $pattern, $newPath)
    }

    if ($updated -cne $original) {
        $edits[$file.FullName] = $updated
    }
}

Write-Host "CMake files inspected: $($cmakeFiles.Count)" -ForegroundColor Cyan
Write-Host "CMake files requiring changes: $($edits.Count)" -ForegroundColor Cyan
foreach ($path in ($edits.Keys | Sort-Object)) {
    Write-Host "  $($path.Substring($root.Length + 1))"
}

# Paths in these files are sometimes used at runtime and cannot be inferred
# safely from their file name alone. Report them, rather than guessing.
$reviewPaths = @()
$reviewFiles = @(Get-ChildItem -LiteralPath $root -File -Recurse |
    Where-Object {
        $_.Extension -in @('.ps1', '.cpp', '.hpp', '.h', '.md', '.json') -and
        $_.FullName -notmatch '[\\/](build|out|\.git|\.vs|vendor|third_party|CMakeFiles)[\\/]'
    })
foreach ($file in $reviewFiles) {
    $content = [IO.File]::ReadAllText($file.FullName)
    foreach ($oldPath in $pathMap.Keys) {
        if ($content.Contains($oldPath)) {
            $reviewPaths += "$($file.FullName.Substring($root.Length + 1)): $oldPath"
        }
    }
}

# Check for include directives still naming relocated flat headers.
$headerIssues = @()
foreach ($group in $headerGroups.Keys) {
    foreach ($header in $headerGroups[$group]) {
        $moved = "include/$group/$header"
        if (-not (Test-Path -LiteralPath (Get-Absolute $moved) -PathType Leaf)) {
            continue
        }
        foreach ($file in $reviewFiles) {
            if ($file.Extension -notin @('.cpp', '.hpp', '.h')) { continue }
            $content = [IO.File]::ReadAllText($file.FullName)
            if ($content -match ('(?m)^\s*#\s*include\s*"(?:\.\./)*(?:src/|include/)?' + [regex]::Escape($header) + '"')) {
                $headerIssues += "$($file.FullName.Substring($root.Length + 1)): $header"
            }
        }
    }
}

if ($Apply) {
    foreach ($filePath in $edits.Keys) {
        $originalBytes = [IO.File]::ReadAllBytes($filePath)
        $hasBom = $originalBytes.Length -ge 3 -and $originalBytes[0] -eq 239 -and $originalBytes[1] -eq 187 -and $originalBytes[2] -eq 191
        $utf8 = New-Object System.Text.UTF8Encoding($hasBom)
        [IO.File]::WriteAllText($filePath, $edits[$filePath], $utf8)
    }
    Write-Host 'CMake source-path updates applied.' -ForegroundColor Green
}
else {
    Write-Host 'Preview only; no files changed. Rerun with -Apply.' -ForegroundColor Yellow
}

if ($reviewPaths.Count -gt 0) {
    Write-Warning 'Old source/test path references outside CMake require review:'
    $reviewPaths | Sort-Object -Unique | ForEach-Object { Write-Host "  $_" }
}
if ($headerIssues.Count -gt 0) {
    Write-Warning 'Potential stale include directives require review:'
    $headerIssues | Sort-Object -Unique | ForEach-Object { Write-Host "  $_" }
}
if ($reviewPaths.Count -eq 0 -and $headerIssues.Count -eq 0) {
    Write-Host 'No known stale direct references detected outside CMake.' -ForegroundColor Green
}
