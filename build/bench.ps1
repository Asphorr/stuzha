param([string]$n = "1", [int]$runs = 1, [switch]$fs, [string]$exe = "stuzha.exe", [int]$skip = 30, [int]$threads = 0)
# --shotN --bench: сцена в живом окне, по каждому проходу минимум, медиана и p90 по всем
# кадрам (медиана устойчива к фону: виртуалка, браузер); fps — по медиане полного кадра.
# Слоты: 0..7 проходы рендера, 8 — передача кадра потоку вывода, 9 — update,
# 10 — поток вывода (надписи + окно, идёт параллельно), 11 — весь кадр, 12..19 — части
$names = @{ 12 = "ntab+moon"; 1 = "lightgrid"; 2 = "floor"; 3 = "objects"; 4 = "roofs"; 0 = "wires+wait"; 18 = "(sky bg)"; 5 = "light";
            14 = "  bloom"; 15 = "  cones"; 16 = "  parts"; 17 = "  flakes"; 6 = "  frost+hb"; 7 = "hud"; 8 = "handoff"; 9 = "update"; 10 = "(output bg)" }
$order = 12, 1, 2, 3, 4, 0, 18, 5, 14, 15, 16, 17, 6, -2, 7, 8, 9, 10
$rows = [System.Collections.Generic.List[double[]]]::new()
Push-Location $PSScriptRoot
for ($r = 0; $r -lt $runs; $r++) {
  $a = @("--shot$n", "--bench"); if ($fs) { $a += "--fs" }; if ($threads) { $a += "--threads $threads" }
  Remove-Item "$PSScriptRoot\bench.bin" -ErrorAction SilentlyContinue
  $p = Start-Process ".\$exe" -ArgumentList $a -Wait -PassThru
  if ($p.ExitCode -ne 0 -or -not (Test-Path "$PSScriptRoot\bench.bin")) { Pop-Location; throw ("run {0}: exit 0x{1:X8}, no bench.bin" -f $r, $p.ExitCode) }
  $d = [IO.File]::ReadAllBytes("$PSScriptRoot\bench.bin")
  $cnt = [BitConverter]::ToInt32($d, 0); $ns = [BitConverter]::ToInt32($d, 4); $q = [BitConverter]::ToInt64($d, 8)
  for ($f = $skip; $f -lt $cnt; $f++) {
    $row = [double[]]::new(26)
    for ($i = 0; $i -lt $ns; $i++) { $row[$i] = [BitConverter]::ToInt64($d, 16 + ($f * $ns + $i) * 8) * 1000.0 / $q }
    $row[21] = $row[6] + $row[14] + $row[15] + $row[16] + $row[17]              # post
    $row[22] = 0; foreach ($i in 0..7 + 12..17) { $row[22] += $row[$i] }        # render (без фоновых)
    $row[23] = $row[11]                                                         # frame
    $rows.Add($row)
  }
}
Pop-Location
function Stat($k) {
  $v = [double[]]($rows | ForEach-Object { $_[$k] }); [Array]::Sort($v)
  [pscustomobject]@{ min = $v[0]; med = $v[[int]($v.Count / 2)]; p90 = $v[[int]($v.Count * 0.9)] }
}
function Line($name, $k) { $s = Stat $k; "{0,-12} {1,7:N2} {2,7:N2} {3,7:N2}" -f $name, $s.min, $s.med, $s.p90 }
"{0,-12} {1,7} {2,7} {3,7}" -f "", "min", "median", "p90"
foreach ($k in $order) {
  if ($k -eq -1) { Line "sky" 20; continue }
  if ($k -eq -2) { Line "post" 21; continue }
  Line $names[$k] $k
}
Line "render" 22
$s = Stat 23; "{0,-12} {1,7:N2} {2,7:N2} {3,7:N2}   => {4:N0} fps (median), {5} frames" -f "frame", $s.min, $s.med, $s.p90, (1000 / $s.med), $rows.Count
