<#
.SYNOPSIS
    ESP32-DOOM Build, Flash, and Monitor Script

.DESCRIPTION
    PowerShell script to build, flash, and monitor ESP32-DOOM project.
    Supports WiFi streaming configuration.

.PARAMETER Action
    Action to perform: build, flash, flash-all, monitor, run, clean, help

.PARAMETER SSID
    WiFi network name for streaming

.PARAMETER Password
    WiFi password for streaming

.PARAMETER Port
    Serial port (default: COM3)

.PARAMETER Baud
    Flash baud rate (default: 460800)

.PARAMETER WadFile
    WAD file to flash (default: data/DOOM1_MINI.WAD)

.EXAMPLE
    .\make.ps1 build -SSID "MyNetwork" -Password "MyPassword"
    .\make.ps1 flash -Port COM5
    .\make.ps1 run -SSID "MyNetwork" -Password "MyPassword"
    .\make.ps1 help
#>

param(
    [Parameter(Position=0)]
    [ValidateSet("build", "flash", "flash-all", "monitor", "run", "clean", "fullclean", "help")]
    [string]$Action = "build",
    
    [string]$SSID = "Amirak-2.4GHz",
    [string]$Password = "135792468",
    [string]$Port = "COM3",
    [int]$Baud = 460800,
    [string]$WadFile = "data/DOOM1.WAD"
)

$ErrorActionPreference = "Stop"

# ESP-IDF Configuration
$IDF_PATH = "D:\ProgramData\esp-idf-v5.3.4"
$ESP_TOOLS = "$IDF_PATH\tools\tools"

# Setup environment
function Setup-Environment {
    $env:IDF_PATH = $IDF_PATH
    $env:IDF_TARGET = "esp32"
    $env:IDF_PYTHON_ENV_PATH = "$IDF_PATH\tools\python_env\idf5.3_py3.11_env"
    $env:PYTHON = "$IDF_PATH\tools\python_env\idf5.3_py3.11_env\Scripts\python.exe"
    $env:ESP_ROM_ELF_DIR = "$IDF_PATH\components\esp_rom\esp32\ld"
    $env:PATH = @(
        "$IDF_PATH\tools\python_env\idf5.3_py3.11_env\Scripts",
        "$ESP_TOOLS\xtensa-esp-elf\esp-13.2.0_20240530\xtensa-esp-elf\bin",
        "$ESP_TOOLS\ninja\1.12.1",
        "$ESP_TOOLS\cmake\3.30.2\bin",
        "$ESP_TOOLS\idf-python\3.11.2",
        "$ESP_TOOLS\idf-python\3.11.2\Scripts",
        "$IDF_PATH\tools",
        $env:PATH
    ) -join ";"
}

# Build function
function Invoke-Build {
    Write-Host "=== Building ESP32-DOOM ===" -ForegroundColor Cyan
    Write-Host "WiFi SSID: $SSID" -ForegroundColor Green
    
    Setup-Environment
    
    # Clean CMake cache for fresh config
    if (Test-Path "build\CMakeCache.txt") {
        Remove-Item "build\CMakeCache.txt" -Force
    }
    
    # Configure
    $cmakeArgs = @(
        "-G", "Ninja",
        "-DCMAKE_TOOLCHAIN_FILE=$IDF_PATH\tools\cmake\toolchain-esp32.cmake",
        "-DIDF_TARGET=esp32",
        "-DDOOM_STREAMING=ON",
        "-DWIFI_SSID=$SSID",
        "-DWIFI_PASSWORD=$Password",
        "-B", "build",
        "-S", "."
    )
    
    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed" }
    
    # Build
    & cmake --build build
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
    
    Write-Host ""
    Write-Host "=== BUILD SUCCESS ===" -ForegroundColor Green
    Write-Host "Binary: build/esp32-doom.bin" -ForegroundColor Yellow
}

# Flash firmware function
function Invoke-Flash {
    param([switch]$IncludeWad)
    
    Write-Host "=== Flashing to $Port ===" -ForegroundColor Cyan
    
    Setup-Environment
    
    $flashArgs = @(
        "-m", "esptool",
        "--chip", "esp32",
        "-p", $Port,
        "-b", $Baud,
        "write_flash",
        "--flash_mode", "dio",
        "--flash_freq", "80m",
        "--flash_size", "16MB",
        "0x1000", "build/bootloader/bootloader.bin",
        "0x8000", "build/partition_table/partition-table.bin",
        "0x10000", "build/esp32-doom.bin"
    )
    
    if ($IncludeWad) {
        $flashArgs += @("0x210000", $WadFile)
    }
    
    & python @flashArgs
    if ($LASTEXITCODE -ne 0) { throw "Flash failed" }
    
    Write-Host "=== FLASH SUCCESS ===" -ForegroundColor Green
}

# Monitor function
function Invoke-Monitor {
    Write-Host "=== Starting Monitor on $Port ===" -ForegroundColor Cyan
    Write-Host "Press Ctrl+] to exit" -ForegroundColor Yellow
    
    Setup-Environment
    
    & python -m esp_idf_monitor -p $Port -b 115200 build/esp32-doom.elf
}

# Clean function
function Invoke-Clean {
    Write-Host "=== Cleaning build ===" -ForegroundColor Cyan
    
    if (Test-Path "build") {
        Remove-Item -Recurse -Force "build"
        Write-Host "Build directory removed" -ForegroundColor Green
    } else {
        Write-Host "Build directory not found" -ForegroundColor Yellow
    }
}

# Full clean function
function Invoke-FullClean {
    Invoke-Clean
    if (Test-Path "sdkconfig") {
        Remove-Item -Force "sdkconfig"
        Write-Host "sdkconfig removed" -ForegroundColor Green
    }
}

# Help function
function Show-Help {
    Write-Host ""
    Write-Host "ESP32-DOOM Build System" -ForegroundColor Cyan
    Write-Host "=======================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Usage: .\make.ps1 <action> [options]" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Actions:" -ForegroundColor Green
    Write-Host "  build      - Build firmware (default)"
    Write-Host "  flash      - Flash firmware only"
    Write-Host "  flash-all  - Flash firmware + WAD file"
    Write-Host "  monitor    - Start serial monitor"
    Write-Host "  run        - Build, flash-all, and monitor"
    Write-Host "  clean      - Remove build directory"
    Write-Host "  fullclean  - Remove build and sdkconfig"
    Write-Host "  help       - Show this help"
    Write-Host ""
    Write-Host "Options:" -ForegroundColor Green
    Write-Host "  -SSID <name>       WiFi network name"
    Write-Host "  -Password <pass>   WiFi password"
    Write-Host "  -Port <port>       Serial port (default: COM3)"
    Write-Host "  -Baud <rate>       Flash baud rate (default: 460800)"
    Write-Host "  -WadFile <path>    WAD file (default: data/DOOM1_MINI.WAD)"
    Write-Host ""
    Write-Host "Examples:" -ForegroundColor Green
    Write-Host '  .\make.ps1 build -SSID "MyNetwork" -Password "MyPass"'
    Write-Host '  .\make.ps1 flash-all -Port COM5'
    Write-Host '  .\make.ps1 run -SSID "MyNetwork" -Password "MyPass"'
    Write-Host ""
    Write-Host "After boot, open: http://<ESP32_IP>/" -ForegroundColor Yellow
    Write-Host ""
}

# Main execution
try {
    Push-Location $PSScriptRoot
    
    switch ($Action) {
        "build"     { Invoke-Build }
        "flash"     { Invoke-Flash }
        "flash-all" { Invoke-Flash -IncludeWad }
        "monitor"   { Invoke-Monitor }
        "run"       { Invoke-Build; Invoke-Flash -IncludeWad; Invoke-Monitor }
        "clean"     { Invoke-Clean }
        "fullclean" { Invoke-FullClean }
        "help"      { Show-Help }
    }
}
catch {
    Write-Host "ERROR: $_" -ForegroundColor Red
    exit 1
}
finally {
    Pop-Location
}
