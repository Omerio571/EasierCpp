param([Parameter(Mandatory=$true)][string]$App)
$ErrorActionPreference = 'Stop'
$App = (Resolve-Path $App).Path
$root = Join-Path ([IO.Path]::GetTempPath()) ('CppLiveLab Stage2 ' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $root | Out-Null
$originalPath = $env:PATH
$checks = 0
function Assert($Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
    $script:checks++
}
function Invoke-Lab([string[]]$Arguments, [string]$InputText = '', [int]$Expected = 0) {
    $start = New-Object System.Diagnostics.ProcessStartInfo
    $start.FileName = $App
    $start.Arguments = ($Arguments | ForEach-Object { '"' + $_.Replace('"', '\"') + '"' }) -join ' '
    $start.UseShellExecute = $false
    $start.RedirectStandardInput = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.EnvironmentVariables['PATH'] = ''
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $start
    [void]$process.Start()
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    $process.StandardInput.Write($InputText)
    $process.StandardInput.Close()
    if (-not $process.WaitForExit(120000)) { $process.Kill(); throw 'Child timed out' }
    $output = $stdout.Result + $stderr.Result
    $exitCode = $process.ExitCode
    $process.Dispose()
    if ($Expected -eq -1) { Assert ($exitCode -ne 0) "Expected failure: $Arguments`n$output" }
    else { Assert ($exitCode -eq $Expected) "Unexpected exit ${exitCode}: $Arguments`n$output" }
    return $output
}
try {
    $hello = Join-Path $root 'Hello Project'
    Invoke-Lab @('create', $hello) | Out-Null
    Assert (Test-Path "$hello/CMakeLists.txt") 'Project CMake file missing'
    Assert ((Get-Content "$hello/CMakeLists.txt" -Raw) -match 'CMAKE_CXX_STANDARD 20') 'Not C++20'
    $before = Get-Content "$hello/src/main.cpp" -Raw
    Invoke-Lab @('create', $hello) '' -1 | Out-Null
    Assert ((Get-Content "$hello/src/main.cpp" -Raw) -ceq $before) 'Create overwrote existing source'
    foreach ($config in @('Debug', 'Release')) {
        $out = Invoke-Lab @('run', $hello, '--config', $config)
        Assert ($out -match 'Hello from C\+\+20!') "Generated project failed in $config"
        Assert (Test-Path "$hello/.cpplivelab/build-$config/app.exe") 'Configuration executable missing'
        Assert ((Get-Content "$hello/.cpplivelab/build-$config/CMakeCache.txt" -Raw) -match "CMAKE_BUILD_TYPE:STRING=$config") 'Wrong config'
    }
    $calc = Join-Path $root 'Calculator Demo'
    Invoke-Lab @('create', $calc, '--demo', 'calculator') | Out-Null
    $out = Invoke-Lab @('run', $calc) "Ada Lovelace`n2.5`n3.75`n"
    Assert ($out -match 'Your name:.*First number:.*Second number:.*Hello, Ada Lovelace! Sum: 6.25') 'Calculator input/output incorrect'
    $out = Invoke-Lab @('run', $calc) "Ada`ninvalid`n" 1
    Assert ($out -match 'Invalid number') 'Invalid numeric input not reported'
    Assert (Test-Path "$calc/src/calculator.cpp") 'Multi-file calculator missing'
    $single = Join-Path $root 'single input.cpp'
    @'
#include <iostream>
#include <string>
#include <span>
int main() {
    int numbers[] = {4, 5};
    std::span<int> values(numbers);
    std::string name;
    std::cout << "Name: ";
    std::getline(std::cin, name);
    std::cout << "Single " << name << ": " << values[0] + values[1] << '\n';
}
'@ | Set-Content $single
    foreach ($config in @('Debug', 'Release')) {
        $out = Invoke-Lab @('run', $single, '--config', $config) "Grace Hopper`n"
        Assert ($out -match 'Single Grace Hopper: 9') "Single-file C++20 I/O failed in $config"
    }
    $custom = Join-Path $root 'Existing CMake'
    New-Item -ItemType Directory "$custom/src" | Out-Null
    "cmake_minimum_required(VERSION 3.20)`nproject(Existing LANGUAGES CXX)`nset(CMAKE_CXX_STANDARD 20)`nadd_subdirectory(src)" | Set-Content "$custom/CMakeLists.txt"
    'add_executable(custom main.cpp answer.cpp)' | Set-Content "$custom/src/CMakeLists.txt"
    '#include <iostream>', 'int answer();', 'int main() { std::cout << "Custom answer: " << answer() << "\n"; return 7; }' | Set-Content "$custom/src/main.cpp"
    'int answer() { return 42; }' | Set-Content "$custom/src/answer.cpp"
    Invoke-Lab @('build', $custom, '--config', 'Release') | Out-Null
    $out = Invoke-Lab @('run', $custom, '--config', 'Release', '--exe', 'src/custom.exe') '' 7
    Assert ($out -match 'Custom answer: 42') 'Existing multi-file project or exit propagation failed'
    Invoke-Lab @('run', $custom) '' -1 | Out-Null
    'int main() { THIS_IS_A_COMPILER_ERROR }' | Set-Content $single
    $out = Invoke-Lab @('run', $single) '' -1
    Assert ($out -match 'error C[0-9]+' -and $out -match 'Compilation failed' -and $out -notmatch 'Running:') 'Compiler error/stale executable handling incorrect'
    Invoke-Lab @('build', (Join-Path $root 'missing.cpp')) '' -1 | Out-Null
    Invoke-Lab @('build', $hello, '--config', 'Wrong') '' 2 | Out-Null
    Invoke-Lab @('build', $hello, '--config') '' 2 | Out-Null
    Invoke-Lab @('build', $hello, '--unknown', 'value') '' 2 | Out-Null
    $managerChild = Join-Path (Split-Path (Split-Path (Split-Path $App))) 'ForbiddenUserProject'
    Invoke-Lab @('create', $managerChild) '' -1 | Out-Null
    Assert (-not (Test-Path $managerChild)) 'User project was created within manager'
    Assert ($env:PATH -ceq $originalPath) 'Parent PATH changed'
    Write-Output "Stage 2 integration: $checks checks passed (empty child PATH; Debug/Release; console I/O; single/multi-file; diagnostics; exit codes; project separation)."
} finally {
    $resolvedRoot = [IO.Path]::GetFullPath($root)
    $resolvedTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolvedRoot.StartsWith($resolvedTemp) -and (Split-Path $resolvedRoot -Leaf).StartsWith('CppLiveLab Stage2 ')) {
        Remove-Item -LiteralPath $resolvedRoot -Recurse -Force
    }
}
