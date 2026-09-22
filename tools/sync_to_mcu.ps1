<#
.SYNOPSIS
  MUI 库 ⇄ 单片机工程 双向同步（信息通道）
.DESCRIPTION
  推送（默认方向）：PC 侧是 MUI 库的唯一开发源头，把 src\ 与 assets\ 下的
    .c/.h 同步到目标工程 middleware\mui\，并在目标端写 SYNC_INFO.txt
    （同步时间 + 库文件指纹），对端凭它即可判断手上是不是最新库。
    只增改、不删除：目标端独有文件（如 port\mui_port_lcd.c）不受影响。
    同时把本仓库 app\app_ui.c/.h（界面）推送到目标工程 app\，
    使单片机界面与模拟器显示保持一致；对端 app\ 下其它文件不受影响。

  拉取（-Pull）：单片机工程的界面 app\app_ui.c/.h 是那边演化的成果，
    拉到本仓库 app\ 后模拟器显示的即是单片机实际界面。
    该文件原样复用、不作任何修改——它对 BSP_LCD_WIDTH/BSP_LCD_HEIGHT 与
    bsp_system_tick_ms() 的依赖，由 simulator\bsp_lcd.h、simulator\bsp_system.h
    两个兼容桩吸收。
.PARAMETER Target
  单片机工程根目录，默认 E:\WorkSpace\Hk-RLT-001
.PARAMETER DryRun
  只比较并列出差异，不改动任何文件
.PARAMETER Pull
  反向拉取：把单片机工程的界面文件同步到本仓库 app\
.EXAMPLE
  .\tools\sync_to_mcu.ps1 -DryRun     # 看看库改了什么，不落地
  .\tools\sync_to_mcu.ps1             # 库 → 单片机工程
  .\tools\sync_to_mcu.ps1 -Pull       # 单片机界面 → 模拟器
#>

[CmdletBinding()]
param(
    [string]$Target = "E:\WorkSpace\Hk-RLT-001",
    [switch]$DryRun,
    [switch]$Pull
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot

if (-not (Test-Path $Target)) {
    Write-Host "目标工程不存在：$Target" -ForegroundColor Red
    exit 2
}

$added   = New-Object System.Collections.ArrayList
$updated = New-Object System.Collections.ArrayList
$same    = 0

if ($Pull) {
    # -------- 反向：单片机界面 → 本仓库 app\ --------
    $srcDir = Join-Path $Target "app"
    $dstDir = Join-Path $Root "app"
    Write-Host ""
    Write-Host "=== 单片机界面 → 模拟器：$(if ($DryRun) { '演练（未改动任何文件）' } else { '实际同步' }) ===" -ForegroundColor Cyan
    Write-Host "源  ：$srcDir"
    Write-Host "目标：$dstDir"
    Write-Host ""

    foreach ($n in @("app_ui.c", "app_ui.h")) {
        $s = Join-Path $srcDir $n
        $d = Join-Path $dstDir $n
        if (-not (Test-Path $s)) {
            Write-Host "对端缺少文件：$s" -ForegroundColor Red
            exit 2
        }
        if (-not (Test-Path $d)) {
            [void]$added.Add($n)
            if (-not $DryRun) { Copy-Item -Path $s -Destination $d -Force }
        } elseif ((Get-FileHash -Algorithm SHA256 -Path $s).Hash -ne
                  (Get-FileHash -Algorithm SHA256 -Path $d).Hash) {
            [void]$updated.Add($n)
            if (-not $DryRun) { Copy-Item -Path $s -Destination $d -Force }
        } else {
            $same++
        }
    }

    if ($added.Count -gt 0) {
        Write-Host "新增 $($added.Count) 个：" -ForegroundColor Green
        $added | ForEach-Object { Write-Host "  + $_" }
    }
    if ($updated.Count -gt 0) {
        Write-Host "更新 $($updated.Count) 个：" -ForegroundColor Yellow
        $updated | ForEach-Object { Write-Host "  * $_" }
    }
    if ($added.Count -eq 0 -and $updated.Count -eq 0) {
        Write-Host "模拟器界面已与单片机一致。" -ForegroundColor Green
    }
    Write-Host "未变 $same 个"
    exit 0
}

# -------- 正向：本仓库 src\ / assets\ → 单片机工程 middleware\mui\ --------
$Maps = @(
    @{ Name = "src";    Src = (Join-Path $Root "src");    Dst = (Join-Path $Target "middleware\mui\src") },
    @{ Name = "assets"; Src = (Join-Path $Root "assets"); Dst = (Join-Path $Target "middleware\mui\assets") }
)

foreach ($m in $Maps) {
    if (-not (Test-Path $m.Src)) {
        Write-Host "源目录不存在：$($m.Src)" -ForegroundColor Red
        exit 2
    }
    if (-not (Test-Path $m.Dst) -and -not $DryRun) {
        New-Item -ItemType Directory -Path $m.Dst -Force | Out-Null
    }
    $files = Get-ChildItem -Path $m.Src -File | Where-Object { $_.Extension -in @(".c", ".h") }
    foreach ($f in $files) {
        $dstFile = Join-Path $m.Dst $f.Name
        $rel = "$($m.Name)\$($f.Name)"
        if (-not (Test-Path $dstFile)) {
            [void]$added.Add($rel)
            if (-not $DryRun) { Copy-Item -Path $f.FullName -Destination $dstFile -Force }
        } elseif ((Get-FileHash -Algorithm SHA256 -Path $f.FullName).Hash -ne
                  (Get-FileHash -Algorithm SHA256 -Path $dstFile).Hash) {
            [void]$updated.Add($rel)
            if (-not $DryRun) { Copy-Item -Path $f.FullName -Destination $dstFile -Force }
        } else {
            $same++
        }
    }
}

# -------- 应用界面：本仓库 app\ → 目标工程 app\ --------
# 界面即"模拟器所见即单片机所示"的那份代码，必须随库一起同步；
# 只推界面文件，对端 app\ 下的 app_main.c / app_tasks.c 等不受影响。
$AppNames = @("app_ui.c", "app_ui.h")
$appSrc = Join-Path $Root "app"
$appDst = Join-Path $Target "app"

if (-not (Test-Path $appDst) -and -not $DryRun) {
    New-Item -ItemType Directory -Path $appDst -Force | Out-Null
}
foreach ($n in $AppNames) {
    $s = Join-Path $appSrc $n
    $d = Join-Path $appDst $n
    if (-not (Test-Path $s)) { continue }
    if (-not (Test-Path $d)) {
        [void]$added.Add("app\$n")
        if (-not $DryRun) { Copy-Item -Path $s -Destination $d -Force }
    } elseif ((Get-FileHash -Algorithm SHA256 -Path $s).Hash -ne
              (Get-FileHash -Algorithm SHA256 -Path $d).Hash) {
        [void]$updated.Add("app\$n")
        if (-not $DryRun) { Copy-Item -Path $s -Destination $d -Force }
    } else {
        $same++
    }
}

$mode = if ($DryRun) { "演练（未改动任何文件）" } else { "实际同步" }
Write-Host ""
Write-Host "=== MUI 库 → 单片机工程：$mode ===" -ForegroundColor Cyan
Write-Host "源  ：$Root"
Write-Host "目标：$Target\middleware\mui"
Write-Host ""

if ($added.Count -gt 0) {
    Write-Host "新增 $($added.Count) 个：" -ForegroundColor Green
    $added | ForEach-Object { Write-Host "  + $_" }
}
if ($updated.Count -gt 0) {
    Write-Host "更新 $($updated.Count) 个：" -ForegroundColor Yellow
    $updated | ForEach-Object { Write-Host "  * $_" }
}
if ($added.Count -eq 0 -and $updated.Count -eq 0) {
    Write-Host "库已是最新，无需同步。" -ForegroundColor Green
}
Write-Host "未变 $same 个"

if (-not $DryRun -and ($added.Count -gt 0 -or $updated.Count -gt 0)) {
    $lines = New-Object System.Collections.ArrayList
    [void]$lines.Add("MUI 库同步记录")
    [void]$lines.Add("同步时间：$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')")
    [void]$lines.Add("源目录  ：$Root")
    [void]$lines.Add("目标目录：$Target\middleware\mui")
    [void]$lines.Add("本次同步：新增 $($added.Count) 个，更新 $($updated.Count) 个，未变 $same 个")
    [void]$lines.Add("")
    if ($added.Count -gt 0) {
        [void]$lines.Add("新增：")
        foreach ($n in $added) { [void]$lines.Add("  + $n") }
    }
    if ($updated.Count -gt 0) {
        [void]$lines.Add("更新：")
        foreach ($n in $updated) { [void]$lines.Add("  * $n") }
    }
    [void]$lines.Add("")
    [void]$lines.Add("库文件指纹（SHA256 前 16 位）：")
    foreach ($m in $Maps) {
        if (-not (Test-Path $m.Dst)) { continue }
        $dfiles = Get-ChildItem -Path $m.Dst -File |
                  Where-Object { $_.Extension -in @(".c", ".h") } | Sort-Object Name
        foreach ($f in $dfiles) {
            $h = (Get-FileHash -Algorithm SHA256 -Path $f.FullName).Hash
            [void]$lines.Add(("  {0}\{1}  {2}" -f $m.Name, $f.Name, $h.Substring(0, 16)))
        }
    }
    [void]$lines.Add("")
    [void]$lines.Add("核对方法：在 PC 侧重新运行 tools\sync_to_mcu.ps1 -DryRun，")
    [void]$lines.Add("若无「新增/更新」输出，即说明两端库完全一致。")

    $infoPath = Join-Path $Target "middleware\mui\SYNC_INFO.txt"
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($infoPath, ($lines -join "`r`n") + "`r`n", $utf8)
    Write-Host ""
    Write-Host "已写入同步记录：$infoPath" -ForegroundColor Cyan
}

if ($added.Count -gt 0) {
    Write-Host ""
    Write-Host "提示：新增文件需在 Keil 工程 project\yc3121.uvprojx 中手动加入编译。" -ForegroundColor Magenta
}