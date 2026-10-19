@echo off
rem Count lines in all .h/.cpp files under this repo (skips build output folders).
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$skip = '\\(Intermediate|Binaries|DerivedDataCache|Saved|x64|\.vs|\.git|\.dump)\\';" ^
  "$files = Get-ChildItem -Recurse -File -Include *.h,*.cpp | Where-Object { $_.FullName -notmatch $skip };" ^
  "$root = (Get-Location).Path + '\';" ^
  "$rows = $files | ForEach-Object { $l = [IO.File]::ReadAllLines($_.FullName); $rel = $_.FullName.Substring($root.Length); $p = $rel.Split('\'); $g = if ($p.Length -ge 3) { $p[0..2] -join '\' } else { $p[0] }; [pscustomobject]@{ Group = $g; Lines = $l.Count; NonBlank = @($l | Where-Object { $_.Trim() -ne '' }).Count } };" ^
  "$rows | Group-Object Group | ForEach-Object { [pscustomobject]@{ Folder = $_.Name; Files = $_.Count; Lines = ($_.Group | Measure-Object Lines -Sum).Sum; NonBlank = ($_.Group | Measure-Object NonBlank -Sum).Sum } } | Sort-Object Lines -Descending | Format-Table -AutoSize;" ^
  "Write-Host ('Total: {0} files, {1} lines, {2} non-blank lines' -f $rows.Count, ($rows | Measure-Object Lines -Sum).Sum, ($rows | Measure-Object NonBlank -Sum).Sum)"
echo.
pause
