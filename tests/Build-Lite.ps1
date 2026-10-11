#requires -Version 7.4
param(
    [ValidateSet('Release (lite)', 'Debug (lite)')]
    [string]$Configuration = 'Release (lite)'
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$root = Split-Path $PSScriptRoot -Parent
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -all -products '*' -property installationPath | Select-Object -First 1
if (!$vs) { throw '未找到 Visual Studio 工具集' }
$msbuild = Join-Path $vs 'MSBuild\Current\Bin\amd64\MSBuild.exe'
$log = Join-Path $root ('Bin\build-' + [guid]::NewGuid().ToString('N') + '.log')
New-Item -ItemType Directory -Path (Split-Path $log -Parent) -Force | Out-Null
$oldPath = $env:PATH
$oldLocation = Get-Location
try {
    #显式提供原项目的预构建批处理查找路径，不修改项目文件。
    $env:PATH = (Join-Path $root 'TrafficMonitor') + ';' + (Split-Path $vswhere -Parent) + ';' + $oldPath
    Set-Location -LiteralPath (Join-Path $root 'TrafficMonitor')
    & $msbuild (Join-Path $root 'TrafficMonitor_Lite.sln') "-p:Configuration=$Configuration" '-p:Platform=x64' '-p:PlatformToolset=v145' '-m' '-v:minimal' *> $log
    $result = $LASTEXITCODE
    if ($result -ne 0) {
        Select-String -LiteralPath $log -Pattern '\berror\b' | Select-Object -First 12
        throw "编译失败，退出码 $result，日志：$log"
    }
    Write-Output "Build passed: $Configuration x64"
    Write-Output "Executable: $(Join-Path $root "Bin\x64\$Configuration\TrafficMonitor.exe")"
    Write-Output "Log: $log"
} finally {
    $env:PATH = $oldPath
    Set-Location -LiteralPath $oldLocation.Path
}
