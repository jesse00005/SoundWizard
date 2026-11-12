param([switch]$ConfigureOnly)
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
if (!$vs) { throw 'Visual Studio C++ tools were not found.' }
$compiler = Get-ChildItem "$vs\VC\Tools\MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$sdkRoot = "${env:ProgramFiles(x86)}\Windows Kits\10"
$sdk = Get-ChildItem "$sdkRoot\Include" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$env:PATH = "$($compiler.FullName)\bin\Hostx64\x64;$sdkRoot\bin\$($sdk.Name)\x64;$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;$env:PATH"
$env:INCLUDE = "$($compiler.FullName)\include;$sdkRoot\Include\$($sdk.Name)\ucrt;$sdkRoot\Include\$($sdk.Name)\shared;$sdkRoot\Include\$($sdk.Name)\um;$sdkRoot\Include\$($sdk.Name)\winrt"
$env:LIB = "$($compiler.FullName)\lib\x64;$sdkRoot\Lib\$($sdk.Name)\ucrt\x64;$sdkRoot\Lib\$($sdk.Name)\um\x64"
cmake -S $taskRoot -B "$taskRoot\build\release" -G Ninja -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
if (!$ConfigureOnly) {
    cmake --build "$taskRoot\build\release" --parallel 4
    if ($LASTEXITCODE) { throw 'Compilation failed.' }
    ctest --test-dir "$taskRoot\build\release" --output-on-failure
    if ($LASTEXITCODE) { throw 'Sound Wizard checks failed.' }
}
