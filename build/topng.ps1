param([string]$n)
Add-Type -AssemblyName System.Drawing
foreach($i in $n.Split(",")){
  $b=[System.Drawing.Bitmap]::FromFile("$PSScriptRoot\shot$i.bmp")
  $s=New-Object System.Drawing.Bitmap 1280,720
  $g=[System.Drawing.Graphics]::FromImage($s)
  $g.InterpolationMode='NearestNeighbor'; $g.PixelOffsetMode='Half'
  $g.DrawImage($b,0,0,1280,720)
  $s.Save("$PSScriptRoot\shot$i.png")
  $g.Dispose(); $b.Dispose(); $s.Dispose()
}
