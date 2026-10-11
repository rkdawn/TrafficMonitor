#requires -Version 7.4
param(
    [Parameter(Mandatory)][string]$SourceInstall,
    [Parameter(Mandatory)][string]$BuildDirectory,
    [string]$ConfigDirectory,
    [string]$Destination
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$SourceInstall = (Resolve-Path -LiteralPath $SourceInstall).Path
$BuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
if (!(Test-Path -LiteralPath (Join-Path $BuildDirectory 'TrafficMonitor.exe'))) {
    throw '构建目录中没有 TrafficMonitor.exe'
}
#完整硬件监控用户不能在迁移过程中被悄悄降级为 Lite。
if (Test-Path -LiteralPath (Join-Path $SourceInstall 'OpenHardwareMonitorApi.dll')) {
    foreach ($name in @('OpenHardwareMonitorApi.dll', 'LibreHardwareMonitorLib.dll')) {
        if (!(Test-Path -LiteralPath (Join-Path $BuildDirectory $name))) {
            throw "构建缺少 $name；请使用完整版产物迁移此安装"
        }
    }
}
$sourceGlobal = Join-Path $SourceInstall 'global_cfg.ini'
$appDataConfig = Join-Path ([Environment]::GetFolderPath('ApplicationData')) 'TrafficMonitor'
$portable = !(Test-Path -LiteralPath (Join-Path $appDataConfig 'config.ini'))
if (Test-Path -LiteralPath $sourceGlobal) {
    $globalText = Get-Content -LiteralPath $sourceGlobal -Raw
    $setting = [regex]::Match($globalText, '(?im)^\s*portable_mode\s*=\s*(true|false|0|1)\s*$')
    if ($setting.Success) { $portable = $setting.Groups[1].Value -match '^(true|1)$' }
}
if (!$ConfigDirectory) {
    $ConfigDirectory = if ($portable) { $SourceInstall } else { $appDataConfig }
}
$ConfigDirectory = (Resolve-Path -LiteralPath $ConfigDirectory).Path
if (!(Test-Path -LiteralPath (Join-Path $ConfigDirectory 'config.ini'))) {
    throw '找不到有效的原版配置文件'
}
if (!$Destination) {
    $Destination = Join-Path $root ('Bin\PersonalTrial-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
}
$Destination = [IO.Path]::GetFullPath($Destination)
if (Test-Path -LiteralPath $Destination) { throw '目标目录已存在；拒绝覆盖，请指定新目录' }
New-Item -ItemType Directory -Path $Destination | Out-Null
Get-ChildItem -LiteralPath $BuildDirectory -Force | Copy-Item -Destination $Destination -Recurse

$records = [Collections.Generic.List[object]]::new()
function Copy-VerifiedFile([string]$Source, [string]$Target) {
    $hash = (Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash
    $parent = Split-Path $Target -Parent
    if (!(Test-Path -LiteralPath $parent)) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
    Copy-Item -LiteralPath $Source -Destination $Target -Force
    if ((Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash -ne $hash) {
        throw "复制期间文件发生变化，请在原程序保存设置后重试：$Source"
    }
    $records.Add([pscustomobject]@{ source = $Source; destination = $Target; sha256 = $hash })
}
function Copy-VerifiedDirectory([string]$Source, [string]$Target) {
    if (!(Test-Path -LiteralPath $Source)) { return }
    foreach ($file in Get-ChildItem -LiteralPath $Source -File -Recurse) {
        $relative = [IO.Path]::GetRelativePath($Source, $file.FullName)
        Copy-VerifiedFile $file.FullName (Join-Path $Target $relative)
    }
}
foreach ($name in @('config.ini', 'history_traffic.dat', 'history_traffic.dat.bak')) {
    $source = Join-Path $ConfigDirectory $name
    if (Test-Path -LiteralPath $source) { Copy-VerifiedFile $source (Join-Path $Destination $name) }
}
Copy-VerifiedDirectory (Join-Path $SourceInstall 'skins') (Join-Path $Destination 'skins')
Copy-VerifiedDirectory (Join-Path $SourceInstall 'plugins') (Join-Path $Destination 'plugins')
#非便携安装的插件设置位于 AppData，覆盖来自安装目录的旧设置副本。
if ($ConfigDirectory -ne $SourceInstall) {
    Copy-VerifiedDirectory (Join-Path $ConfigDirectory 'plugins') (Join-Path $Destination 'plugins')
}
#仅写新目录：让试用版独立保存配置，绝不共享原版配置目录。
$utf8 = [Text.UTF8Encoding]::new($true)
function Write-AtomicText([string]$Path, [string]$Text) {
    $temporary = Join-Path (Split-Path $Path -Parent) ([guid]::NewGuid().ToString('N') + '.tmp')
    [IO.File]::WriteAllText($temporary, $Text, $utf8)
    [IO.File]::Move($temporary, $Path, $true)
}
Write-AtomicText (Join-Path $Destination 'global_cfg.ini') "[config]`r`nportable_mode=true`r`n"
$manifest = [ordered]@{
    sourceInstall = $SourceInstall
    configDirectory = $ConfigDirectory
    copiedFiles = $records
    note = '仅复制配置、流量历史、皮肤和现有插件；不升级插件，不启动程序，不修改原版或自启动项。此记录含本机路径，只保留在本地。'
}
Write-AtomicText (Join-Path $Destination 'migration.local.json') ($manifest | ConvertTo-Json -Depth 5)
Write-Output "Prepared: $Destination"
Write-Output "Verified personal files: $($records.Count)"
