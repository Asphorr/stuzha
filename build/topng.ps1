param([string]$n)
# снимки -> PNG: viewN.bmp (кадр с HUD, как в окне) — как есть; нет его — shotN.bmp x2 ближайшим
Add-Type -AssemblyName System.Drawing
foreach($i in $n.Split(",")){
  $v="$PSScriptRoot\view$i.bmp"
  if(Test-Path $v){
    $b=[System.Drawing.Bitmap]::FromFile($v)
    $b.Save("$PSScriptRoot\shot$i.png",[System.Drawing.Imaging.ImageFormat]::Png)
    $b.Dispose()
    continue
  }
  $b=[System.Drawing.Bitmap]::FromFile("$PSScriptRoot\shot$i.bmp")
  $s=New-Object System.Drawing.Bitmap 1280,720
  $g=[System.Drawing.Graphics]::FromImage($s)
  $g.InterpolationMode='NearestNeighbor'; $g.PixelOffsetMode='Half'
  $g.DrawImage($b,0,0,1280,720)
  $s.Save("$PSScriptRoot\shot$i.png",[System.Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $b.Dispose(); $s.Dispose()
}
