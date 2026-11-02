param([string]$Helper)
$ErrorActionPreference='Stop'
. $Helper
$root=Join-Path $env:TEMP ('EasierCpp-cache-test-' + [guid]::NewGuid().ToString('N'))
$repo=Join-Path $root 'EasierCpp'
$directory=Join-Path $repo 'build/integrated-tests'
function Require($value,$reason){if(-not $value){throw $reason}}
function Cache($path,$recordedSource,$recorded,$name='EasierCpp'){
    [IO.Directory]::CreateDirectory($path) | Out-Null
    [IO.File]::WriteAllText((Join-Path $path 'CMakeCache.txt'),
        "CMAKE_PROJECT_NAME:STATIC=$name`nCMAKE_HOME_DIRECTORY:INTERNAL=$recordedSource`nCMAKE_CACHEFILE_DIR:INTERNAL=$recorded`nCMAKE_BUILD_TYPE:STRING=Debug`nCMAKE_GENERATOR:INTERNAL=Ninja`n")
}
try {
    [IO.Directory]::CreateDirectory($directory) | Out-Null
    Require ((Resolve-EasierCppTestBuild $repo $directory) -eq $directory) 'Fresh build changed'
    Cache $directory $repo $directory
    $validBytes=[IO.File]::ReadAllText((Join-Path $directory 'CMakeCache.txt'))
    Require ((Resolve-EasierCppTestBuild $repo $directory) -eq $directory) 'Valid incremental cache replaced'
    Require (([IO.File]::ReadAllText((Join-Path $directory 'CMakeCache.txt'))) -ceq $validBytes) 'Valid cache modified'
    Cache $directory (Join-Path $root 'CppLiveLab') (Join-Path $root 'CppLiveLab/build/integrated-tests') 'CppLiveLab'
    $oldBytes=[IO.File]::ReadAllText((Join-Path $directory 'CMakeCache.txt'))
    $fresh=Resolve-EasierCppTestBuild $repo $directory
    Require ($fresh -ne $directory) 'Relocated legacy cache was not recovered'
    Require (([IO.File]::ReadAllText((Join-Path $directory 'CMakeCache.txt'))) -ceq $oldBytes) 'Stale cache was modified'
    Cache $fresh $repo $fresh
    Require ((Resolve-EasierCppTestBuild $repo $directory) -eq $fresh) 'Recovery cache was not reused'
    # Wrong build location alone is also stale, even if the source path is current.
    Cache $directory $repo (Join-Path $root 'old build')
    Require ((Resolve-EasierCppTestBuild $repo $directory) -eq $fresh) 'Build-directory mismatch ignored'
    Cache $directory $repo $directory 'UserProject'
    $refused=$false
    try { Resolve-EasierCppTestBuild $repo $directory | Out-Null } catch { $refused=$true }
    Require $refused 'Foreign project cache accepted'
    $external=Join-Path $root 'UserProject/build'
    Cache $external (Join-Path $root 'CppLiveLab') $external 'CppLiveLab'
    $refused=$false
    try { Resolve-EasierCppTestBuild $repo $external | Out-Null } catch { $refused=$true }
    Require $refused 'Outside build directory recovered without ownership'
    Write-Output 'EasierCpp cache fixtures: fresh, valid, relocated source/build, recovery reuse, foreign/outside refusal passed'
} finally {
    $resolved=[IO.Path]::GetFullPath($root)
    if ((Split-Path $resolved -Parent) -eq [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') -and
        (Split-Path $resolved -Leaf) -like 'EasierCpp-cache-test-*') {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
