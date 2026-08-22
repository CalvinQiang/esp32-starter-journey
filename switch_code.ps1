<#
.SYNOPSIS
    ESP32 物联网实战闯关 —— 示例代码一键切换工具 (Windows PowerShell 原生版)
.DESCRIPTION
    用法:
      .\switch_code.ps1 list                     # 列出所有可用的关卡与实验
      .\switch_code.ps1 <关卡号> [实验号]         # 切换指定实验代码到 main/app_main.c
      .\switch_code.ps1 <关卡号> [实验号] --build # 切换并立即编译
      .\switch_code.ps1 <关卡号> [实验号] --flash # 切换并立即烧录监控
#>

param (
    [Parameter(Position=0)]
    [string]$Arg1,

    [Parameter(Position=1)]
    [string]$Arg2,

    [Parameter(Position=2)]
    [string]$Arg3
)

$ErrorActionPreference = "Stop"

$ProjectRoot = $PSScriptRoot
$CodeDir = Join-Path $ProjectRoot "code"
$TargetMain = Join-Path $ProjectRoot "main\app_main.c"

function Print-Banner {
    Write-Host "=================================================================" -ForegroundColor Cyan
    Write-Host "   🛠️  ESP32 实战教程 —— 示例代码一键切换工具 (switch_code)" -ForegroundColor Cyan
    Write-Host "=================================================================" -ForegroundColor Cyan
}

function Print-Usage {
    Print-Banner
    Write-Host "📖 用法:" -ForegroundColor Yellow
    Write-Host "  .\switch_code.ps1 list                     # 列出所有可用的关卡与实验"
    Write-Host "  .\switch_code.ps1 <关卡号> [实验号]         # 切换指定实验代码到 main/app_main.c"
    Write-Host "  .\switch_code.ps1 <关卡号> [实验号] --build # 切换并立即编译"
    Write-Host "  .\switch_code.ps1 <关卡号> [实验号] --flash # 切换并立即烧录监控"
    Write-Host ""
    Write-Host "💡 示例:" -ForegroundColor Green
    Write-Host "  .\switch_code.ps1 1                        # 切换到 Level 01: 串口通信"
    Write-Host "  .\switch_code.ps1 8 1                      # 切换到 Level 08 实验 1: I2C Scanner"
    Write-Host "  .\switch_code.ps1 8 1 --flash              # 切换并立即烧录"
    Write-Host "=================================================================" -ForegroundColor Cyan
}

function List-Experiments {
    Print-Banner
    Write-Host "📂 当前可供切换运行的示例代码列表：`n" -ForegroundColor Green

    $chapters = @(
        @{ Dir="01_hello_world"; Title="第 01 关: 串口通信与 Hello World" },
        @{ Dir="02_gpio_pwm"; Title="第 02 关: GPIO 输出与 PWM 呼吸灯" },
        @{ Dir="03_gpio_input"; Title="第 03 关: 按键输入检测与人体红外感应" },
        @{ Dir="04_gpio_interrupt"; Title="第 04 关: GPIO 外部中断与事件驱动" },
        @{ Dir="05_freertos_queue"; Title="第 05 关: FreeRTOS 多任务与双核队列" },
        @{ Dir="06_ws2812_rmt"; Title="第 06 关: RMT 硬件脉冲与 WS2812 幻彩 RGB" },
        @{ Dir="07_adc_ultrasonic"; Title="第 07 关: ADC 模拟量采集与超声波测距" },
        @{ Dir="08_i2c_dht11"; Title="第 08 关: I2C 通信总线与 DHT11 温湿度" },
        @{ Dir="09_nvs_storage"; Title="第 09 关: NVS 存储与 Flash 偏好设置" },
        @{ Dir="10_st7789_display"; Title="第 10 关: ST7789 彩屏与几何图形渲染" },
        @{ Dir="11_lvgl_touch"; Title="第 11 关: LVGL 图形框架与电容触摸" },
        @{ Dir="12_wifi_weather"; Title="第 12 关: WiFi 联网与 HTTP 天气时钟" },
        @{ Dir="13_mqtt_iot"; Title="第 13 关: MQTT 物联网通信与手机远程控制" },
        @{ Dir="14_ble_gatt"; Title="第 14 关: BLE 蓝牙广播与手机透传遥控" },
        @{ Dir="15_sdcard_fatfs"; Title="第 15 关: TF 卡 SDIO 驱动与 FatFS 文件系统" },
        @{ Dir="16_low_power_deepsleep"; Title="第 16 关: 低功耗电源管理与 Deep-sleep 休眠" },
        @{ Dir="17_software_architecture"; Title="第 17 关: 嵌入式分层架构与事件总线" },
        @{ Dir="18_final_station_hub"; Title="第 18 关: 桌面智能气象站与物联网超级中控台" }
    )

    foreach ($ch in $chapters) {
        $chPath = Join-Path $CodeDir $ch.Dir
        Write-Host "▶ $($ch.Title)" -ForegroundColor Cyan -NoNewline
        Write-Host "  (目录: code/$($ch.Dir))" -ForegroundColor Gray

        if (Test-Path $chPath) {
            $files = Get-ChildItem -Path $chPath -Filter "*.c" | Sort-Object Name
            $idx = 1
            foreach ($f in $files) {
                $firstLine = ""
                if (Test-Path $f.FullName) {
                    $content = Get-Content -Path $f.FullName -TotalCount 10
                    $match = $content | Where-Object { $_ -match "🌟" } | Select-Object -First 1
                    if ($match) { $firstLine = $match.Trim() }
                }
                $shortPrefix = $ch.Dir.Substring(0, 2)
                Write-Host "   [" -NoNewline
                Write-Host "$shortPrefix $idx" -ForegroundColor Yellow -NoNewline
                Write-Host "] $($f.Name)  " -NoNewline
                Write-Host "$firstLine" -ForegroundColor DarkGray
                $idx++
            }
        }
        Write-Host ""
    }
    Write-Host "=================================================================" -ForegroundColor Cyan
}

# 1. 判断是否是 list 或 help
if ($Arg1 -eq "list" -or $Arg1 -eq "--list" -or $Arg1 -eq "-l" -or $Arg1 -eq "-list") {
    List-Experiments
    exit 0
}

if ([string]::IsNullOrWhiteSpace($Arg1) -or $Arg1 -eq "help" -or $Arg1 -eq "--help" -or $Arg1 -eq "-h") {
    Print-Usage
    exit 0
}

# 2. 解析关卡号与实验号
$chNum = $Arg1
if ($chNum -match '^\d+$') {
    $chNum = "{0:D2}" -f [int]$chNum
}

$expNum = 1
$actionFlag = ""

if (-not [string]::IsNullOrWhiteSpace($Arg2)) {
    if ($Arg2 -match '^\d+$') {
        $expNum = [int]$Arg2
    } elseif ($Arg2.StartsWith("-")) {
        $actionFlag = $Arg2
    }
}

if (-not [string]::IsNullOrWhiteSpace($Arg3)) {
    if ($Arg3.StartsWith("-")) {
        $actionFlag = $Arg3
    }
}

# 查找对应关卡目录
$matchDirs = Get-ChildItem -Path $CodeDir -Directory | Where-Object { $_.Name -like "${chNum}_*" }
if (-not $matchDirs -or $matchDirs.Count -eq 0) {
    Write-Host "`n❌ 错误：未找到第 $chNum 关对应的代码目录！" -ForegroundColor Red
    Write-Host "使用 '.\switch_code.ps1 list' 查看全部可用关卡。" -ForegroundColor Yellow
    exit 1
}

$targetDir = $matchDirs[0].FullName
$expFiles = Get-ChildItem -Path $targetDir -Filter "*.c" | Sort-Object Name
if (-not $expFiles -or $expFiles.Count -eq 0) {
    Write-Host "`n❌ 错误：目录 $($matchDirs[0].Name) 下没有 .c 源码文件！" -ForegroundColor Red
    exit 1
}

if ($expNum -gt $expFiles.Count -or $expNum -lt 1) {
    Write-Host "`n❌ 错误：第 $chNum 关只有 $($expFiles.Count) 个实验，你指定的实验号是 $expNum！" -ForegroundColor Red
    exit 1
}

$targetSrc = $expFiles[$expNum - 1].FullName
Copy-Item -Path $targetSrc -Destination $TargetMain -Force

$relSrc = $targetSrc.Replace($ProjectRoot, "").TrimStart("\/")
Write-Host "=================================================================" -ForegroundColor Green
Write-Host "✅ 成功切换示例代码！" -ForegroundColor Green
Write-Host "   源文件: " -NoNewline
Write-Host "$relSrc" -ForegroundColor Yellow
Write-Host "   目标位: " -NoNewline
Write-Host "main/app_main.c" -ForegroundColor Cyan
Write-Host "=================================================================" -ForegroundColor Green

# 检查是否需要编译或烧录
if ($actionFlag -eq "--build" -or $actionFlag -eq "-b") {
    Write-Host "`n🔨 正在执行编译 (idf.py build)..." -ForegroundColor Cyan
    idf.py build
} elseif ($actionFlag -eq "--flash" -or $actionFlag -eq "-f") {
    Write-Host "`n⚡ 正在执行烧录与串口监视 (idf.py flash monitor)..." -ForegroundColor Cyan
    idf.py flash monitor
}
