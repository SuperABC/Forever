@echo off
REM Run Forever in game mode (not Editor). Window stays open (pause at end)
REM so a fast failure is readable instead of flashing by.

set ENGINE_EXE=C:\Workspace\UE5\Engine\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe
set PROJECT=%~dp0Forever.uproject

echo Engine: %ENGINE_EXE%
echo Project: %PROJECT%
echo.

if not exist "%ENGINE_EXE%" (
    echo [ERROR] Engine executable not found. Check the ENGINE_EXE path above.
    pause
    exit /b 1
)
if not exist "%PROJECT%" (
    echo [ERROR] Forever.uproject not found. Check the PROJECT path above.
    pause
    exit /b 1
)

echo Starting...
"%ENGINE_EXE%" "%PROJECT%" -game -windowed -resx=1280 -resy=720 -log
set EXITCODE=%ERRORLEVEL%

echo.
echo Exited with code: %EXITCODE%

