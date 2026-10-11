#requires -Version 7.4
param(
    [switch]$Baseline,
    [string]$Repository = (Split-Path $PSScriptRoot -Parent)
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -all -products '*' -property installationPath | Select-Object -First 1
if (!$vs) { throw '未找到 Visual Studio 工具集' }
$mode = if ($Baseline) { 'baseline' } else { 'fixed' }
$stage = Join-Path $Repository ('Bin\TaskbarPositionTests\' + $mode + '-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
$utf8 = [System.Text.UTF8Encoding]::new($false)
foreach ($name in @('Win11TaskbarDlg.cpp', 'Win11TaskbarDlg.h')) {
    $target = Join-Path $stage $name
    if ($Baseline) {
        #PowerShell 7.4+ 的原生命令重定向保留原始字节，避免损坏 UTF-8 BOM。
        & git -C $Repository show "HEAD:TrafficMonitor/$name" > $target
        if ($LASTEXITCODE -ne 0) { throw "获取基线失败：$name" }
    } else {
        Copy-Item -LiteralPath (Join-Path $Repository "TrafficMonitor\$name") -Destination $target
    }
}
#编译未经改写的生产定位代码，仅从 support 目录提供应用设置和基类接口。
$devcmd = Join-Path $vs 'Common7\Tools\VsDevCmd.bat'
$toolVersion = (Get-Content -LiteralPath (Join-Path $vs 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt') -Raw).Trim()
$atl = Join-Path $vs "VC\Tools\MSVC\$toolVersion\atlmfc\include"
$subject = Join-Path $stage 'Win11TaskbarDlg.cpp'
$test = Join-Path $PSScriptRoot 'TaskbarPositionTests.cpp'
$support = Join-Path $PSScriptRoot 'support'
$exe = Join-Path $stage 'TaskbarPositionTests.exe'
$buildLog = Join-Path $stage 'build.log'
$testLog = Join-Path $stage 'test.log'
$cmd = Join-Path $stage 'build.cmd'
$lines = @(
    '@echo off',
    "set `"PATH=$(Split-Path $vswhere -Parent);%PATH%`"",
    "call `"$devcmd`" -arch=x64 -host_arch=x64 >nul",
    'if errorlevel 1 exit /b %errorlevel%',
    "cd /d `"$stage`"",
    "cl /nologo /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE /W4 /WX /I`"$stage`" /I`"$support`" /I`"$atl`" `"$subject`" `"$test`" /Fe:`"$exe`" /link user32.lib gdi32.lib",
    'exit /b %errorlevel%'
)
[System.IO.File]::WriteAllLines($cmd, $lines, $utf8)
& $env:ComSpec /d /c $cmd *> $buildLog
if ($LASTEXITCODE -ne 0) {
    Get-Content -LiteralPath $buildLog | Select-Object -First 20
    throw "测试编译失败：$buildLog"
}
& $exe *> $testLog
$testExit = $LASTEXITCODE
Get-Content -LiteralPath $testLog
$metadata = [ordered]@{
    mode = $mode
    sourceSha256 = (Get-FileHash -LiteralPath $subject -Algorithm SHA256).Hash
    testExitCode = $testExit
    buildLog = $buildLog
    testLog = $testLog
    limitation = '真实Win32隐藏窗口测试；基类和设置为测试替身，不等同于Explorer内运行验收'
}
[System.IO.File]::WriteAllText((Join-Path $stage 'result.json'), ($metadata | ConvertTo-Json), $utf8)
Write-Output "Artifacts: $stage"
exit $testExit
