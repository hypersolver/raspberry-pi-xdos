param(
    [string]$Distribution = 'Ubuntu-24.04',
    [switch]$InstallDependencies,
    [switch]$TestDebug
)

$ErrorActionPreference = 'Stop'
$linuxRoot = wsl.exe --distribution $Distribution --exec wslpath -a -u "$PSScriptRoot"
if ($LASTEXITCODE -ne 0 -or -not $linuxRoot) {
    throw "Cannot access WSL distribution '$Distribution'. Check wsl.exe --list --verbose."
}
$linuxRoot = ($linuxRoot | Select-Object -Last 1).Trim()
$buildArguments = @("$linuxRoot/tools/build-wsl.sh", $linuxRoot)
if ($InstallDependencies) {
    $buildArguments += '--install-dependencies'
}
$testDebugValue = [int]$TestDebug.IsPresent
wsl.exe --distribution $Distribution --exec env "XDOS_TEST_DEBUG=$testDebugValue" bash @buildArguments
exit $LASTEXITCODE
