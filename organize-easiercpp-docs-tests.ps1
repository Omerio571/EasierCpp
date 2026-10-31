param(
    [switch]$Apply
)

$ErrorActionPreference = 'Stop'
$root = (Get-Location).Path

if (-not (Test-Path -LiteralPath (Join-Path $root 'CMakeLists.txt'))) {
    throw 'Run this script from the EasierCpp repository root.'
}

# Keep repository entry points and build conventions at the root.
# Categorize historical documents without changing their content.
$documentGroups = @{
    'docs/planning' = @(
        'EASIERCPP_V1_IMPLEMENTATION_PLAN.md',
        'EASIERCPP_V1_PHASE1_AUDIT.md',
        'PROJECT_REDIRECTION_PLAN.md'
    )
    'docs/reports/auto-run' = @(
        'AUTO_RUN_OPTIMIZATION_REPORT.md',
        'AUTO_RUN_RELIABILITY_REPORT.md',
        'AUTO_RUN_SPEED_REPORT.md'
    )
    'docs/reports/libraries' = @('LIBRARY_RELIABILITY_FIX_REPORT.md')
    'docs/reports/maintenance' = @('MAINTENANCE_REVIEW.md')
    'docs/reports/project-redirection' = @('PROJECT_REDIRECTION_REPORT.md')
    'docs/reports/stage4' = @('STAGE4_FINAL_REFINEMENTS.md', 'STAGE4_REVIEW.md')
    'docs/reports/stage5' = @(
        'STAGE5_CONSOLIDATED_REPORT.md', 'STAGE5_FINAL_REPORT.md',
        'STAGE5_REPORT.md', 'STAGE5_UI_REPORT.md', 'STAGE5_UNIFIED_REPORT.md'
    )
    'docs/reports/ui' = @('UI_AUDIT_REPORT.md')
    'docs/manual-tests/auto-run' = @('AUTO_RUN_RELIABILITY_MANUAL_TESTS.md')
    'docs/manual-tests/maintenance' = @('MAINTENANCE_MANUAL_TESTS.md')
    'docs/manual-tests/project-redirection' = @('PROJECT_REDIRECTION_MANUAL_TESTS.md')
    'docs/manual-tests/stages' = @(
        'STAGE2_MANUAL_TESTS.md', 'STAGE3_MANUAL_TESTS.md',
        'STAGE4_MANUAL_TESTS.md', 'STAGE4_REPAIR_MANUAL_TESTS.md',
        'STAGE5_CONSOLIDATED_MANUAL_TESTS.md', 'STAGE5_FINAL_MANUAL_TESTS.md',
        'STAGE5_MANUAL_TESTS.md', 'STAGE5_UI_MANUAL_TESTS.md',
        'STAGE5_UNIFIED_MANUAL_TESTS.md'
    )
    'docs/manual-tests/ui' = @('UI_AUDIT_MANUAL_TESTS.md')
    'docs/design' = @('THEME_ACCESSIBILITY.md', 'UI_DESIGN_GUIDELINES.md')
    'docs/history' = @('GIT_CHECKPOINT.md', 'TEST_RESULTS.md')
}

# Do not move CMake/PowerShell test drivers yet: their relative file references
# and runtime assumptions warrant a separate integration change.
$testGroups = @{
    'tests/build' = @('stage4_tests.cpp', 'performance_probe.cpp')
    'tests/cli' = @('doctor_tests.cpp')
    'tests/gui' = @(
        'console_view_tests.cpp', 'gui_flow_tests.cpp', 'restart_gui_tests.cpp',
        'ui_audit_tests.cpp', 'ui_forms_tests.cpp'
    )
    'tests/libraries' = @(
        'library_compatibility_tests.cpp', 'library_live_tests.cpp',
        'library_management_tests.cpp'
    )
    'tests/process' = @(
        'redirection_tests.cpp', 'reliability_tests.cpp', 'restart_tests.cpp',
        'session_fixture.cpp', 'session_tests.cpp'
    )
    'tests/projects' = @('stage5_tests.cpp', 'unified_tests.cpp')
    'tests/maintenance' = @('maintenance_tests.cpp')
    'tests/tools' = @('workflow_probe.cpp')
}

function To-RelativePath([string]$fromDir, [string]$toPath) {
    $from = [Uri]((([IO.Path]::GetFullPath($fromDir)).TrimEnd('\', '/')) + [IO.Path]::DirectorySeparatorChar)
    $to = [Uri][IO.Path]::GetFullPath($toPath)
    return [Uri]::UnescapeDataString($from.MakeRelativeUri($to).ToString()).Replace('/', '/')
}

$moves = @{}
foreach ($group in $documentGroups.Keys) {
    foreach ($name in $documentGroups[$group]) {
        $old = ($name -replace '\\', '/')
        $moves[$old] = "$group/$name"
    }
}
foreach ($group in $testGroups.Keys) {
    foreach ($name in $testGroups[$group]) {
        $old = "tests/$name"
        $moves[$old] = "$group/$name"
    }
}

# Only process files that actually exist. This supports local changes ahead of GitHub.
$existingMoves = @{}
foreach ($old in $moves.Keys) {
    if (Test-Path -LiteralPath (Join-Path $root $old.Replace('/', [IO.Path]::DirectorySeparatorChar)) -PathType Leaf) {
        $dest = $moves[$old]
        if (Test-Path -LiteralPath (Join-Path $root $dest.Replace('/', [IO.Path]::DirectorySeparatorChar))) {
            throw "Destination already exists: $dest"
        }
        $existingMoves[$old] = $dest
    }
}

Write-Host "Proposed moves: $($existingMoves.Count)" -ForegroundColor Cyan
foreach ($old in ($existingMoves.Keys | Sort-Object)) {
    Write-Host "  $old  ->  $($existingMoves[$old])"
}

# Before moving, adjust only quoted includes in relocated tests where the old
# include resolved to a real file. Do not rewrite public API includes that rely
# on the configured include/ search path.
$includeEdits = @{}
foreach ($old in $existingMoves.Keys) {
    if (-not $old.EndsWith('.cpp')) { continue }
    $source = Join-Path $root $old.Replace('/', [IO.Path]::DirectorySeparatorChar)
    $destination = Join-Path $root $existingMoves[$old].Replace('/', [IO.Path]::DirectorySeparatorChar)
    $text = [IO.File]::ReadAllText($source)
    $oldDirectory = Split-Path $source -Parent
    $newDirectory = Split-Path $destination -Parent
    $pattern = '(?m)^(\s*#\s*include\s*")([^"\r\n]+)(")'
    $updated = [regex]::Replace($text, $pattern, [System.Text.RegularExpressions.MatchEvaluator]{
        param($match)
        $requested = $match.Groups[2].Value
        $resolved = [IO.Path]::GetFullPath((Join-Path $oldDirectory $requested.Replace('/', [IO.Path]::DirectorySeparatorChar)))
        if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) { return $match.Value }
        $relativeTarget = $resolved.Substring($root.TrimEnd('\').Length).TrimStart('\').Replace('\', '/')
        if ($existingMoves.ContainsKey($relativeTarget)) {
            $resolved = Join-Path $root $existingMoves[$relativeTarget].Replace('/', [IO.Path]::DirectorySeparatorChar)
        }
        $newInclude = To-RelativePath $newDirectory $resolved
        return $match.Groups[1].Value + $newInclude + $match.Groups[3].Value
    })
    if ($updated -cne $text) { $includeEdits[$old] = $updated }
}

Write-Host "Test source files needing relative include updates: $($includeEdits.Count)" -ForegroundColor Cyan

# Update literal CMake paths to moved test source files, leaving target names,
# labels and build behavior unchanged. Other script/docs references are reported.
$cmakeFiles = @(Get-ChildItem -LiteralPath $root -Filter 'CMakeLists.txt' -File) +
    @(Get-ChildItem -LiteralPath $root -Recurse -File -Filter '*.cmake' |
        Where-Object { $_.FullName -notmatch '[\\/](build|\.git|\.vs|vendor|third_party)[\\/]' })
$cmakeEdits = @{}
foreach ($file in $cmakeFiles) {
    $content = [IO.File]::ReadAllText($file.FullName)
    $updated = $content
    foreach ($old in $existingMoves.Keys) {
        if (-not $old.StartsWith('tests/')) { continue }
        $updated = $updated.Replace($old, $existingMoves[$old])
    }
    if ($updated -cne $content) { $cmakeEdits[$file.FullName] = $updated }
}
Write-Host "CMake files needing test source path updates: $($cmakeEdits.Count)" -ForegroundColor Cyan

if (-not $Apply) {
    Write-Host 'Preview only. No files were changed. Run with -Apply to proceed.' -ForegroundColor Yellow
    exit 0
}

foreach ($old in ($existingMoves.Keys | Sort-Object)) {
    $from = Join-Path $root $old.Replace('/', [IO.Path]::DirectorySeparatorChar)
    $to = Join-Path $root $existingMoves[$old].Replace('/', [IO.Path]::DirectorySeparatorChar)
    $toDirectory = Split-Path $to -Parent
    [IO.Directory]::CreateDirectory($toDirectory) | Out-Null
    Move-Item -LiteralPath $from -Destination $to
    if ($includeEdits.ContainsKey($old)) {
        [IO.File]::WriteAllText($to, $includeEdits[$old])
    }
}
foreach ($file in $cmakeEdits.Keys) {
    [IO.File]::WriteAllText($file, $cmakeEdits[$file])
}

# Find references requiring human review (README links, script file lookups,
# documentation cross-links, and unusual CMake variables).
Write-Host 'Potential old-path references to review:' -ForegroundColor Yellow
$reviewFiles = @(Get-ChildItem $root -Recurse -File -Include *.md,*.ps1,*.cmake,CMakeLists.txt |
    Where-Object { $_.FullName -notmatch '[\\/](build|\.git|\.vs|vendor|third_party)[\\/]' })
$reported = 0
foreach ($file in $reviewFiles) {
    $content = [IO.File]::ReadAllText($file.FullName)
    foreach ($old in $existingMoves.Keys) {
        if ($content.Contains($old) -or ($old.EndsWith('.md') -and $content.Contains([IO.Path]::GetFileName($old)))) {
            Write-Host "  $($file.FullName): $old"
            $reported++
        }
    }
}
if ($reported -eq 0) { Write-Host '  No exact references found.' }
Write-Host 'Done. Review references, configure CMake, build, and run Quick/Full tests.' -ForegroundColor Green
