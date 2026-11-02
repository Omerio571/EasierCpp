param(
    [switch]$Check,
    [string]$ClangFormat
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path $PSScriptRoot -Parent

if (-not $ClangFormat) {
    $onPath = Get-Command clang-format.exe -ErrorAction SilentlyContinue
    if ($onPath) {
        $ClangFormat = $onPath.Source
    } else {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        if (Test-Path -LiteralPath $vswhere) {
            $candidates = & $vswhere -all -products '*' -find 'VC\Tools\Llvm\x64\bin\clang-format.exe'
            $ClangFormat = $candidates | Select-Object -First 1
        }
    }
}

if (-not $ClangFormat -or -not (Test-Path -LiteralPath $ClangFormat)) {
    throw 'clang-format was not found. Install the Visual Studio C++ Clang tools or pass -ClangFormat <exe-path>.'
}

& $ClangFormat --version
if ($LASTEXITCODE -ne 0) { throw 'Cannot run clang-format.' }

# Validate the configuration before touching files. Unknown options cause failure.
& $ClangFormat --style=file --assume-filename "$repositoryRoot/src/main.cpp" --dump-config | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'This clang-format version cannot read .clang-format.' }

# Maintain only our code directories. Never recurse through build/, .vs/, external
# projects or third-party code, even if such files happen to have a C++ extension.
$sourceFiles = foreach ($directory in @('src', 'include', 'tests')) {
    Get-ChildItem -LiteralPath (Join-Path $repositoryRoot $directory) -File -Recurse |
        Where-Object {
            $relativePath = $_.FullName.Substring($repositoryRoot.Length).Replace('\', '/')
            $isGeneratedOrExternal = $relativePath -match '/(out|generated|third_party|vendor|CMakeFiles|\.vs|\.cpplivelab|\.easiercpp)/' -or
                $relativePath -match '^/(src|include|tests)/.+/build/'
            $_.Extension -in @('.cpp', '.hpp', '.h', '.cc', '.cxx') -and -not $isGeneratedOrExternal
        }
}

foreach ($sourceFile in $sourceFiles) {
    if ($Check) {
        & $ClangFormat --style=file --dry-run --Werror $sourceFile.FullName
    } else {
        & $ClangFormat --style=file -i $sourceFile.FullName
    }
    if ($LASTEXITCODE -ne 0) { throw "Formatting failed: $($sourceFile.FullName)" }
}

if ($Check) {
    Write-Output "Formatting verified: $($sourceFiles.Count) maintained C++ files."
} else {
    Write-Output "Formatting applied: $($sourceFiles.Count) maintained C++ files."
}
