$d = [IO.File]::ReadAllBytes("$PSScriptRoot\prof.bin")
$q = [BitConverter]::ToInt64($d, 120)
$names = "sky", "lightgrid", "floor", "objects", "roofs", "light", "post", "hud"
$t = 0
for ($i = 0; $i -lt 8; $i++) {
  $v = [BitConverter]::ToInt64($d, $i * 8) * 1000.0 / $q / 10
  $t += $v
  "{0,-10} {1,6:N2} ms" -f $names[$i], $v
}
"{0,-10} {1,6:N2} ms" -f "total", $t
