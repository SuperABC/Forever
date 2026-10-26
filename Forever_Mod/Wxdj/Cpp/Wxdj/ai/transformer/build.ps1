# 编译训练环境wxenv.dll：直接编译上级目录的battle/源码(规则和系数与游戏一致)，改了battle/之后重新跑一次。
# 用法：powershell -File build.ps1
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$vc = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
New-Item -ItemType Directory -Force (Join-Path $here "build") | Out-Null
$src = "env.cpp ..\..\battle\action.cpp ..\..\battle\controller.cpp ..\..\battle\cultivator.cpp ..\..\battle\effect.cpp ..\..\battle\modifier.cpp"
cmd /c "`"$vc`" >nul 2>&1 & cd /d `"$here`" & cl /nologo /O2 /EHsc /std:c++17 /utf-8 /LD /I ..\..\battle /I . /Fo:build\ /Fe:wxenv.dll $src" 2>&1 | Select-Object -Last 15
if (Test-Path (Join-Path $here "wxenv.dll")) { Write-Host "OK: wxenv.dll" } else { Write-Host "BUILD FAILED"; exit 1 }
