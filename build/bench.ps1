param([string]$n = "1", [int]$runs = 5, [string]$exe = "stuzha.exe")
# минимум по нескольким прогонам --shotN: фон (браузер и т.п.) даёт только лишнее, не меньше
$names = "sky", "lightgrid", "floor", "objects", "roofs", "light", "post", "hud"
$best = @(1e9) * 8
Push-Location $PSScriptRoot
for ($r = 0; $r -lt $runs; $r++) {
  Start-Process ".\$exe" -ArgumentList "--shot$n" -Wait
  $d = [IO.File]::ReadAllBytes("$PSScriptRoot\prof.bin")
  $q = [BitConverter]::ToInt64($d, 120)
  for ($i = 0; $i -lt 8; $i++) {
    $v = [BitConverter]::ToInt64($d, $i * 8) * 1000.0 / $q / 10
    if ($v -lt $best[$i]) { $best[$i] = $v }
  }
}
Pop-Location
$t = 0
for ($i = 0; $i -lt 8; $i++) { $t += $best[$i]; "{0,-10} {1,6:N2} ms" -f $names[$i], $best[$i] }
"{0,-10} {1,6:N2} ms" -f "total", $t
