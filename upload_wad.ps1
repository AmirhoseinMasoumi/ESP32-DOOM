# Upload WAD file to ESP32 SPIFFS
# Place your DOOM1.WAD file in the data/ folder first!

Write-Host "=================================" -ForegroundColor Cyan
Write-Host "ESP32-DOOM WAD Upload Script" -ForegroundColor Cyan
Write-Host "=================================" -ForegroundColor Cyan

# Check if data folder exists
if (-not (Test-Path "data")) {
    Write-Host "`nCreating data folder..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path "data" | Out-Null
}

# Check if WAD file exists
$wadFiles = Get-ChildItem -Path "data" -Filter "*.WAD"
if ($wadFiles.Count -eq 0) {
    Write-Host "`nERROR: No WAD file found in data/ folder!" -ForegroundColor Red
    Write-Host "Please place DOOM1.WAD (shareware) in the data/ folder" -ForegroundColor Yellow
    Write-Host "You can download it from: https://distro.ibiblio.org/slitaz/sources/packages/d/doom1.wad" -ForegroundColor Yellow
    exit 1
}

Write-Host "`nFound WAD files:" -ForegroundColor Green
foreach ($wad in $wadFiles) {
    $sizeMB = [math]::Round($wad.Length / 1MB, 2)
    Write-Host "  - $($wad.Name) ($sizeMB MB)" -ForegroundColor Cyan
    
    # Warn if file is too large
    if ($wad.Length -gt 2500000) {
        Write-Host "    WARNING: File is large, may not fit in SPIFFS!" -ForegroundColor Yellow
    }
}

# Rename to DOOM1.WAD if needed
$mainWad = Join-Path "data" "DOOM1.WAD"
if (-not (Test-Path $mainWad)) {
    $firstWad = $wadFiles[0].FullName
    Write-Host "`nRenaming $($wadFiles[0].Name) to DOOM1.WAD..." -ForegroundColor Yellow
    Copy-Item $firstWad $mainWad
}

# Upload to SPIFFS
Write-Host "`nUploading filesystem to ESP32..." -ForegroundColor Yellow
Write-Host "This may take several minutes depending on WAD size..." -ForegroundColor Gray

pio run --target uploadfs

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nFilesystem uploaded successfully!" -ForegroundColor Green
} else {
    Write-Host "`nUpload FAILED!" -ForegroundColor Red
    Write-Host "Make sure ESP32 is connected and not in use by another program" -ForegroundColor Yellow
    exit 1
}

Write-Host "`nDone! You can now upload the main program with build.ps1" -ForegroundColor Green
