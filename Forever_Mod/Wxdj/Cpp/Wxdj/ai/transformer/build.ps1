# 编译训练环境wxenv.dll：直接编译上级目录的battle/源码(规则和系数与游戏一致)，改了battle/之后重新跑一次。
# 用法：powershell -File build.ps1
param([string]$Out = "wxenv.dll")   # 训练在跑时wxenv.dll被占用，可以 -Out wxenv_dev.dll 先编到别的名字(用环境变量 WXENV_DLL=wxenv_dev.dll 让wxenv.py加载它)
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$vc = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
New-Item -ItemType Directory -Force (Join-Path $here "build_$Out") | Out-Null
$src = "env.cpp ..\..\battle\action.cpp ..\..\battle\controller.cpp ..\..\battle\cultivator.cpp ..\..\battle\effect.cpp ..\..\battle\modifier.cpp"
cmd /c "`"$vc`" >nul 2>&1 & cd /d `"$here`" & cl /nologo /O2 /EHsc /std:c++17 /utf-8 /LD /I ..\..\battle /I . /Fo:build_$Out\ /Fe:$Out $src" 2>&1 | Select-Object -Last 15
if (Test-Path (Join-Path $here $Out)) { Write-Host "OK: $Out" } else { Write-Host "BUILD FAILED"; exit 1 }
