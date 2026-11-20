Add-Type -AssemblyName System.Drawing

$srcPath = "C:\Users\anasr\.gemini\antigravity\brain\724fa737-5a34-49e8-901a-15affb5b8eda\ebooster_app_icon_1795139555533.jpg"
$destPng = Join-Path $PSScriptRoot "..\assets\logo.png"
$destIco = Join-Path $PSScriptRoot "..\assets\app.ico"

Write-Host "Loading source image from: $srcPath" -ForegroundColor Cyan
$srcImg = [System.Drawing.Image]::FromFile($srcPath)

# Save high-res PNG version
$srcImg.Save($destPng, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host "Saved high-res PNG -> $destPng" -ForegroundColor Green

# Standard Windows icon sizes
$sizes = @(256, 128, 64, 48, 32, 16)
$pngDataList = @()

foreach ($sz in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap $sz, $sz
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.DrawImage($srcImg, 0, 0, $sz, $sz)
    $g.Dispose()

    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()

    $pngDataList += ,@($sz, $ms.ToArray())
    $ms.Dispose()
}
$srcImg.Dispose()

# Build valid Windows .ICO with PNG frames
$fs = [System.IO.File]::Create($destIco)
$bw = New-Object System.IO.BinaryWriter $fs

# ICO Header
$bw.Write([uint16]0) # Reserved
$bw.Write([uint16]1) # Type 1 = Icon
$bw.Write([uint16]$pngDataList.Count) # Image count

# Calculate offsets
$headerSize = 6 + ($pngDataList.Count * 16)
$currentOffset = $headerSize

# Directory entries
foreach ($item in $pngDataList) {
    $sz = $item[0]
    $data = $item[1]
    $bSz = if ($sz -eq 256) { [byte]0 } else { [byte]$sz }

    $bw.Write([byte]$bSz)              # Width
    $bw.Write([byte]$bSz)              # Height
    $bw.Write([byte]0)                 # ColorCount
    $bw.Write([byte]0)                 # Reserved
    $bw.Write([uint16]1)               # Color planes
    $bw.Write([uint16]32)              # Bits per pixel
    $bw.Write([uint32]$data.Length)    # Data size
    $bw.Write([uint32]$currentOffset)  # Offset

    $currentOffset += $data.Length
}

# Image data
foreach ($item in $pngDataList) {
    $data = $item[1]
    $bw.Write($data)
}

$bw.Flush()
$bw.Close()
$fs.Close()

$icoItem = Get-Item $destIco
Write-Host "Created valid multi-res Windows icon -> $destIco ($($icoItem.Length) bytes, $($pngDataList.Count) mip levels: 16-256px)" -ForegroundColor Green
