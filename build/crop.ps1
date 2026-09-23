param([string]$n, [int]$x, [int]$y, [int]$w, [int]$h, [int]$s = 4, [string]$out = "crop")
Add-Type -AssemblyName System.Drawing
$src = [System.Drawing.Bitmap]::FromFile("$PSScriptRoot\shot$n.bmp")
$dst = New-Object System.Drawing.Bitmap ($w * $s), ($h * $s)
$g = [System.Drawing.Graphics]::FromImage($dst)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
$g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, ($w * $s), ($h * $s)), (New-Object System.Drawing.Rectangle $x, $y, $w, $h), [System.Drawing.GraphicsUnit]::Pixel)
$dst.Save("$PSScriptRoot\$out.png", [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $dst.Dispose(); $src.Dispose()
