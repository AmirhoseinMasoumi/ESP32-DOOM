# Strip DOOM1.WAD to fit in ESP32 flash
# Removes: All music (D_*), Episodes 2 and 3, some sounds

$inputWad = "DOOM1.WAD"
$outputWad = "DOOM1_MINI.WAD"

Write-Host "Reading $inputWad..."
$wadBytes = [System.IO.File]::ReadAllBytes($inputWad)

# Parse WAD header
$header = [System.Text.Encoding]::ASCII.GetString($wadBytes[0..3])
$numLumps = [BitConverter]::ToInt32($wadBytes, 4)
$dirOffset = [BitConverter]::ToInt32($wadBytes, 8)

Write-Host "WAD Type: $header"
Write-Host "Original lumps: $numLumps"
Write-Host "Directory at: 0x$($dirOffset.ToString('X'))"

# Parse directory entries (16 bytes each: 4=offset, 4=size, 8=name)
$lumps = @()
for ($i = 0; $i -lt $numLumps; $i++) {
    $entryOffset = $dirOffset + ($i * 16)
    $lumpOffset = [BitConverter]::ToInt32($wadBytes, $entryOffset)
    $lumpSize = [BitConverter]::ToInt32($wadBytes, $entryOffset + 4)
    $nameBytes = $wadBytes[($entryOffset + 8)..($entryOffset + 15)]
    $lumpName = [System.Text.Encoding]::ASCII.GetString($nameBytes).TrimEnd([char]0)
    
    $lumps += @{
        Name = $lumpName
        Offset = $lumpOffset
        Size = $lumpSize
    }
}

# Filter out unwanted lumps
$keepLumps = $lumps | Where-Object {
    $name = $_.Name
    # Keep everything EXCEPT:
    -not ($name -match '^D_E\d') -and      # Music tracks (D_E1M1, D_E2M1, etc.)
    -not ($name -match '^D_INTER') -and    # Intermission music
    -not ($name -match '^D_INTROA') -and   # Intro music
    -not ($name -match '^D_VICTOR') -and   # Victory music
    -not ($name -match '^D_BUNNY') -and    # Ending music
    -not ($name -eq 'E2M1') -and           # Episode 2 maps
    -not ($name -eq 'E2M2') -and
    -not ($name -eq 'E2M3') -and
    -not ($name -eq 'E2M4') -and
    -not ($name -eq 'E2M5') -and
    -not ($name -eq 'E2M6') -and
    -not ($name -eq 'E2M7') -and
    -not ($name -eq 'E2M8') -and
    -not ($name -eq 'E2M9') -and
    -not ($name -eq 'E3M1') -and           # Episode 3 maps
    -not ($name -eq 'E3M2') -and
    -not ($name -eq 'E3M3') -and
    -not ($name -eq 'E3M4') -and
    -not ($name -eq 'E3M5') -and
    -not ($name -eq 'E3M6') -and
    -not ($name -eq 'E3M7') -and
    -not ($name -eq 'E3M8') -and
    -not ($name -eq 'E3M9')
}

Write-Host "Keeping $($keepLumps.Count) lumps (removed $($numLumps - $keepLumps.Count))"

# Build new WAD file
$newWadData = New-Object System.Collections.Generic.List[byte]

# Write header (will update later)
$newWadData.AddRange([System.Text.Encoding]::ASCII.GetBytes($header))
$newWadData.AddRange([BitConverter]::GetBytes([int]$keepLumps.Count))
$newWadData.AddRange([BitConverter]::GetBytes([int]12))  # Temp directory offset

# Copy lump data
$currentOffset = 12
foreach ($lump in $keepLumps) {
    if ($lump.Size -gt 0) {
        $lumpData = $wadBytes[$lump.Offset..($lump.Offset + $lump.Size - 1)]
        $lump.NewOffset = $currentOffset
        $newWadData.AddRange($lumpData)
        $currentOffset += $lump.Size
    } else {
        $lump.NewOffset = $currentOffset
    }
}

# Write directory
$directoryOffset = $newWadData.Count
foreach ($lump in $keepLumps) {
    # Offset (4 bytes)
    $newWadData.AddRange([BitConverter]::GetBytes([int]$lump.NewOffset))
    # Size (4 bytes)
    $newWadData.AddRange([BitConverter]::GetBytes([int]$lump.Size))
    # Name (8 bytes, padded with nulls)
    $nameBytes = [System.Text.Encoding]::ASCII.GetBytes($lump.Name.PadRight(8, [char]0))
    $newWadData.AddRange($nameBytes[0..7])
}

# Update directory offset in header
$dirOffsetBytes = [BitConverter]::GetBytes([int]$directoryOffset)
$newWadData[8] = $dirOffsetBytes[0]
$newWadData[9] = $dirOffsetBytes[1]
$newWadData[10] = $dirOffsetBytes[2]
$newWadData[11] = $dirOffsetBytes[3]

# Write output file
[System.IO.File]::WriteAllBytes($outputWad, $newWadData.ToArray())

$outputSize = (Get-Item $outputWad).Length
Write-Host ""
Write-Host "Created $outputWad"
Write-Host "Original size: $($wadBytes.Length) bytes ($([math]::Round($wadBytes.Length/1MB, 2)) MB)"
Write-Host "New size: $outputSize bytes ($([math]::Round($outputSize/1MB, 2)) MB)"
Write-Host "Saved: $($wadBytes.Length - $outputSize) bytes ($([math]::Round(($wadBytes.Length - $outputSize)/1MB, 2)) MB)"

$maxSize = 0x2EF000
if ($outputSize -le $maxSize) {
    Write-Host ""
    Write-Host "SUCCESS! WAD fits in flash partition ($([math]::Round($maxSize/1MB, 2)) MB)" -ForegroundColor Green
} else {
    $diff = $outputSize - $maxSize
    Write-Host ""
    Write-Host "Still $diff bytes too large ($([math]::Round($diff/1MB, 2)) MB)" -ForegroundColor Yellow
}
