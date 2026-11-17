@echo off
setlocal

rem Usage: run_tests.bat [quick^|full] [core^|ue^|package^|all]
rem   quick (default): small map, few seeds, 3 months, 10s perf sample. full: 1024 map, more seeds, 24 months, 30s perf sample.
rem   core: CoreTest.exe (no rendering).
rem   ue:   Source/Test/UE module via -ForeverTest. Headless suites run with -nullrhi; perf and commute run in a window.
rem   package: pak.bat (BuildCookRun: build+cook+stage+pak), then the packaged Forever.exe runs boot,components. Part of "all" in full mode only (slow).
rem   Build first: build.bat (or MSBuild Source\Framework.sln /t:CoreTest + UBT ForeverEditor)

set "ROOT=%~dp0..\.."
set "MODE=%~1"
if "%MODE%"=="" set "MODE=quick"
set "WHICH=%~2"
if "%WHICH%"=="" set "WHICH=all"
set "ENGINE=C:\Workspace\UE5\Engine\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
set "ENGINE_GUI=C:\Workspace\UE5\Engine\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"
set "PERFSEC=10"
if "%MODE%"=="full" set "PERFSEC=30"
set "RESULT=0"
set "STAMP=%DATE:~0,4%%DATE:~5,2%%DATE:~8,2%_%TIME:~0,2%%TIME:~3,2%%TIME:~6,2%"
set "STAMP=%STAMP: =0%"
set "RUNDIR=%ROOT%\Saved\TestReports\%STAMP%"
set "OUT=%RUNDIR%\core"
mkdir "%RUNDIR%" 2>nul

if "%WHICH%"=="ue" goto :ue
if "%WHICH%"=="package" goto :package

if not exist "%ROOT%\x64\Release\CoreTest.exe" (
	echo CoreTest.exe not found, build Framework.sln first.
	exit /b 2
)
echo ==== Core pipeline (%MODE%) ====
pushd "%ROOT%"
x64\Release\CoreTest.exe Resource\Config\config.json --suite all --%MODE% --out "%OUT%"
if errorlevel 1 set "RESULT=1"
popd

:ue
if "%WHICH%"=="core" goto :summary
echo ==== UE pipeline (%MODE%): headless suites ====
"%ENGINE%" "%ROOT%\Forever.uproject" /Game/Levels/World -game -nullrhi -unattended -nosplash -nosound -stdout -ForeverTest=boot,components,saveload -ForeverTestOut="%RUNDIR%\ue" > "%RUNDIR%\ue_headless.log" 2>&1
if errorlevel 1 set "RESULT=1"
rem Time skip and the rush-hour run need the fade widget to tick, which only happens with rendering, so they run in a window.
echo ==== UE pipeline (%MODE%): rendered suites ====
"%ENGINE_GUI%" "%ROOT%\Forever.uproject" /Game/Levels/World -game -windowed -ResX=960 -ResY=540 -unattended -nosplash -nosound -log -ForeverTest=perform,commute,ride -ForeverTestPerfSeconds=%PERFSEC% -ForeverTestOut="%RUNDIR%\ue_rendered"
if errorlevel 1 set "RESULT=1"
rem changes modifies world state (weather, NPCs, vehicles...), so it gets its own process, last.
echo ==== UE pipeline (%MODE%): changes suite ====
"%ENGINE_GUI%" "%ROOT%\Forever.uproject" /Game/Levels/World -game -windowed -ResX=960 -ResY=540 -unattended -nosplash -nosound -log -ForeverTest=changes -ForeverTestOut="%RUNDIR%\ue_changes"
if errorlevel 1 set "RESULT=1"

if "%WHICH%"=="ue" goto :summary
if "%MODE%"=="full" goto :package
if "%WHICH%"=="package" goto :package
goto :summary

:package
echo ==== Package pipeline: pak.bat (build, cook, stage, pak) ====
set "PKGARCH=%ROOT%\Saved\Archive\Windows"
set "PKGOUT=%RUNDIR%\package"
mkdir "%PKGOUT%" 2>nul
call "%ROOT%\pak.bat" > "%PKGOUT%\pak.log" 2>&1
if errorlevel 1 (
	echo PACKAGING FAILED, see %PKGOUT%\pak.log
	set "RESULT=1"
	goto :summary
)
if not exist "%PKGARCH%\Forever.exe" (
	echo PACKAGING FAILED: %PKGARCH%\Forever.exe missing
	set "RESULT=1"
	goto :summary
)
if not exist "%PKGARCH%\Forever\Content\Paks\Forever-Windows.pak" (
	echo PACKAGING FAILED: pak file missing
	set "RESULT=1"
	goto :summary
)
echo ==== Package pipeline: packaged exe boot + components (headless) ====
"%PKGARCH%\Forever.exe" /Game/Levels/World -nullrhi -unattended -nosplash -nosound -stdout -ForeverTest=boot,components -ForeverTestOut="%PKGOUT%" > "%PKGOUT%\packaged.log" 2>&1
if errorlevel 1 (
	echo PACKAGED RUN FAILED, see %PKGOUT%\packaged.log
	set "RESULT=1"
)
rem traffic: the packaged build crashed in Chaos (vehicle destroyed while physics ran) only with a walking player among cars, so run it windowed with rendering.
echo ==== Package pipeline: packaged exe traffic (windowed) ====
"%PKGARCH%\Forever.exe" /Game/Levels/World -windowed -ResX=960 -ResY=540 -unattended -nosplash -nosound -ForeverTest=traffic -ForeverTestTrafficSeconds=180 -ForeverTestOut="%PKGOUT%\traffic" > "%PKGOUT%\packaged_traffic.log" 2>&1
if errorlevel 1 (
	echo PACKAGED TRAFFIC RUN FAILED, see %PKGOUT%\packaged_traffic.log
	set "RESULT=1"
)

:summary
rem Summarize (compares metrics with Saved\TestReports\baseline\core when it exists)
set "BASELINE="
if exist "%ROOT%\Saved\TestReports\baseline\core" set "BASELINE=%ROOT%\Saved\TestReports\baseline\core"
if exist "%OUT%" python "%~dp0summarize.py" "%OUT%" %BASELINE% > nul

echo Reports: %RUNDIR%
exit /b %RESULT%
