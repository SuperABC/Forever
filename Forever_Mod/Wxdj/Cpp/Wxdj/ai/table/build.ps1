# 编译训练程序trainer.exe：直接编译上级目录的battle/源码(规则和系数与游戏一致)，改了battle/之后重新跑一次。
# 用法：powershell -File build.ps1
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$battle = Join-Path $here "..\..\battle"
$obj = Join-Path $here "build"
$vc = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
New-Item -ItemType Directory -Force $obj | Out-Null
$src = "trainer.cpp ..\..\battle\action.cpp ..\..\battle\controller.cpp ..\..\battle\cultivator.cpp ..\..\battle\effect.cpp"
cmd /c "`"$vc`" >nul 2>&1 & cd /d `"$here`" & cl /nologo /O2 /EHsc /std:c++17 /utf-8 /I ..\..\battle /Fo:build\ /Fe:trainer.exe $src" 2>&1 | Select-Object -Last 12
if (Test-Path (Join-Path $here "trainer.exe")) { Write-Host "OK: trainer.exe" } else { Write-Host "BUILD FAILED"; exit 1 }
