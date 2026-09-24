param([string]$n = "1", [int]$runs = 1, [switch]$fs, [string]$exe = "stuzha.exe", [int]$skip = 30, [int]$threads = 0)
# --shotN --bench: сцена в живом окне, по каждому проходу минимум и медиана по всем кадрам
# (медиана устойчива к фону: виртуалка, браузер), fps — по медиане полного кадра
$names = "sky", "lightgrid", "floor", "objects", "roofs", "light", "post", "hud", "present", "update", "x10", "x11"
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
    $row = [double[]]::new(14)
    for ($i = 0; $i -lt $ns; $i++) {
      $row[$i] = [BitConverter]::ToInt64($d, 16 + ($f * $ns + $i) * 8) * 1000.0 / $q
      if ($i -lt 8 -or $i -ge 10) { $row[12] += $row[$i] }
      $row[13] += $row[$i]
    }
    $rows.Add($row)
  }
}
Pop-Location
function Stat($k) {
  $v = [double[]]($rows | ForEach-Object { $_[$k] }); [Array]::Sort($v)
  [pscustomobject]@{ min = $v[0]; med = $v[[int]($v.Count / 2)]; p90 = $v[[int]($v.Count * 0.9)] }
}
"{0,-10} {1,7} {2,7} {3,7}" -f "", "min", "median", "p90"
for ($i = 0; $i -lt 12; $i++) {
  $s = Stat $i
  if ($i -ge 10 -and $s.med -eq 0) { continue }
  "{0,-10} {1,7:N2} {2,7:N2} {3,7:N2}" -f $names[$i], $s.min, $s.med, $s.p90
}
$s = Stat 12; "{0,-10} {1,7:N2} {2,7:N2} {3,7:N2}" -f "render", $s.min, $s.med, $s.p90
$s = Stat 13; "{0,-10} {1,7:N2} {2,7:N2} {3,7:N2}   => {4:N0} fps (median), {5} frames" -f "frame", $s.min, $s.med, $s.p90, (1000 / $s.med), $rows.Count
