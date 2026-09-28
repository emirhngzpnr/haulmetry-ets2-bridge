param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'Visual Studio C++ build tools were not found.' }
$preserved = @{}
foreach ($name in @('USERPROFILE', 'LOCALAPPDATA', 'APPDATA', 'TEMP', 'TMP')) {
    $preserved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
& (Join-Path $installation 'Common7\Tools\Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
foreach ($name in $preserved.Keys) {
    [Environment]::SetEnvironmentVariable($name, $preserved[$name], 'Process')
}
Push-Location $PSScriptRoot
try {
    $preset = 'x64-' + $Configuration.ToLowerInvariant()
    cmake --preset $preset
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    cmake --build "out/build/$preset"
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    & "./out/build/$preset/haulmetry-ets2-bridge/haulmetry-ets2-bridge.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Program failed.' }
} finally {
    Pop-Location
}
