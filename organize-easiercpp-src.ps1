param(
    [switch]$Apply
)

$ErrorActionPreference = "Stop"

# Run from the EasierCpp repository root. Without -Apply, only preview moves.
$repositoryRoot = (Get-Location).Path
$cmakeFile = Join-Path $repositoryRoot "CMakeLists.txt"
$sourceDirectory = Join-Path $repositoryRoot "src"

if (-not (Test-Path -LiteralPath $cmakeFile -PathType Leaf) -or
    -not (Test-Path -LiteralPath $sourceDirectory -PathType Container)) {
    throw "Run this script from the EasierCpp repository root."
}

# Group source files by their primary responsibility, not by executable target.
# GUI-related library manager/workflow files remain under gui because they
# implement UI workflows rather than the core dependency model.
$sourceGroups = [ordered]@{
    "appearance" = @("appearance.cpp", "ui_theme.cpp")
    "build"      = @("auto_build.cpp", "build_freshness.cpp")
    "cli"        = @("workspace_cli.cpp", "doctor.cpp")
    "gui"        = @("gui.cpp", "console_view.cpp", "project_dialogs.cpp", "library_manager.cpp", "library_workflow.cpp")
    "libraries"  = @("libraries.cpp", "library_compatibility.cpp")
    "process"    = @("process.cpp", "application_windows.cpp", "restart.cpp", "session.cpp", "console_model.cpp", "output_decoder.cpp")
    "projects"   = @("projects.cpp", "workspace.cpp", "workspace_creation.cpp")
    "tools"      = @("tool_workflows.cpp", "editors.cpp")
}

$plannedMoves = New-Object System.Collections.Generic.List[object]
foreach ($group in $sourceGroups.Keys) {
    foreach ($fileName in $sourceGroups[$group]) {
        $oldRelative = "src/$fileName"
        $newRelative = "src/$group/$fileName"
        $oldAbsolute = Join-Path $repositoryRoot ($oldRelative.Replace("/", "\"))
        $newAbsolute = Join-Path $repositoryRoot ($newRelative.Replace("/", "\"))

        if (Test-Path -LiteralPath $oldAbsolute -PathType Leaf) {
            if (Test-Path -LiteralPath $newAbsolute) {
                throw "Destination already exists: $newRelative"
            }
            $plannedMoves.Add([pscustomobject]@{
                OldRelative = $oldRelative
                NewRelative = $newRelative
                OldAbsolute = $oldAbsolute
                NewAbsolute = $newAbsolute
            })
        }
        elseif (-not (Test-Path -LiteralPath $newAbsolute -PathType Leaf)) {
            throw "Expected source file not found: $oldRelative"
        }
    }
}

if ($plannedMoves.Count -eq 0) {
    Write-Host "Source files are already organized; nothing to move."
    exit 0
}

# Check the CMake source references before moving any files.
$originalCmake = [IO.File]::ReadAllText($cmakeFile)
$updatedCmake = $originalCmake
foreach ($move in $plannedMoves) {
    $escapedPath = [regex]::Escape($move.OldRelative)
    $pattern = "(?<![A-Za-z0-9_./])$escapedPath(?![A-Za-z0-9_./])"
    $matches = [regex]::Matches($updatedCmake, $pattern)
    if ($matches.Count -eq 0) {
        throw "CMake source path not found: $($move.OldRelative). Review CMakeLists.txt before moving."
    }
    $updatedCmake = [regex]::Replace($updatedCmake, $pattern, $move.NewRelative)
}

Write-Host "Source moves ($($plannedMoves.Count)):"
$plannedMoves | ForEach-Object {
    Write-Host "  $($_.OldRelative) -> $($_.NewRelative)"
}

if (-not $Apply) {
    Write-Host "`nPreview only. Run with -Apply to move files and update CMakeLists.txt."
    exit 0
}

foreach ($move in $plannedMoves) {
    $destinationDirectory = Split-Path $move.NewAbsolute -Parent
    if (-not (Test-Path -LiteralPath $destinationDirectory)) {
        New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null
    }
    Move-Item -LiteralPath $move.OldAbsolute -Destination $move.NewAbsolute
}

# Preserve the original UTF-8 BOM convention if present.
$bytes = [IO.File]::ReadAllBytes($cmakeFile)
$hasBom = $bytes.Length -ge 3 -and $bytes[0] -eq 239 -and $bytes[1] -eq 187 -and $bytes[2] -eq 191
$encoding = New-Object System.Text.UTF8Encoding($hasBom)
[IO.File]::WriteAllText($cmakeFile, $updatedCmake, $encoding)

Write-Host "`nMoved source files and updated explicit CMake source paths."

# Report leftover references outside CMakeLists.txt rather than silently
# rewriting tests, documentation, scripts or generated starter templates.
$oldPaths = $plannedMoves | ForEach-Object { $_.OldRelative }
$searchRoots = @("src", "include", "tests", "scripts")
$remaining = New-Object System.Collections.Generic.List[string]
foreach ($folder in $searchRoots) {
    $folderPath = Join-Path $repositoryRoot $folder
    if (-not (Test-Path $folderPath)) { continue }
    Get-ChildItem -LiteralPath $folderPath -File -Recurse |
        Where-Object { $_.Extension -in @(".cpp", ".hpp", ".h", ".cmake", ".ps1", ".json", ".md") } |
        ForEach-Object {
            $filePath = $_.FullName
            $text = [IO.File]::ReadAllText($filePath)
            foreach ($oldPath in $oldPaths) {
                if ($text.Contains($oldPath) -or $text.Contains($oldPath.Replace("/", "\"))) {
                    $remaining.Add("$filePath : $oldPath")
                }
            }
        }
}

if ($remaining.Count -gt 0) {
    Write-Warning "Other files still reference old source paths. Review these before building:"
    $remaining | ForEach-Object { Write-Host "  $_" }
}
else {
    Write-Host "No old source paths found in maintained source, tests, or scripts."
}

Write-Host "`nNext: configure, build, run Quick tests, then Full tests."
