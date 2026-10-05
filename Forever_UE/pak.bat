@echo off
set ENGINE=C:\Workspace\UE5\Engine\UE_5.7
set PROJECT=C:\Workspace\UE5\Projects\Forever\Forever_UE\Forever.uproject
set ARCHIVE=C:\Workspace\UE5\Projects\Forever\Forever_UE\Saved\Archive
set BINDIR=C:\Workspace\UE5\Projects\Forever\Forever_UE\Binaries\Win64

if exist "%ARCHIVE%" rmdir /s /q "%ARCHIVE%"
if exist "%~dp0Saved\Cooked\Windows" rmdir /s /q "%~dp0Saved\Cooked\Windows"

del /f /q "%BINDIR%\Forever.exe"    2>nul
del /f /q "%BINDIR%\Forever.target" 2>nul
del /f /q "%BINDIR%\Forever.lib"    2>nul
del /f /q "%BINDIR%\Forever.exp"    2>nul

call "%ENGINE%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
  -project="%PROJECT%" -noP4 -platform=Win64 -clientconfig=Development ^
  -build -cook -allmaps -stage -pak -archive ^
  -archivedirectory="%ARCHIVE%"
if errorlevel 1 goto :fail

rem Link (not copy) Resource/ and x64/Release into the archive, next to Binaries/Win64.
rem These change constantly during dev, so a copied snapshot would go stale immediately.
rem mklink /J needs no admin rights (unlike /D symlinks). The archive's project folder is
rem named "Forever" (matches Forever.uproject), not "Forever_UE" (just this repo dir's name)
rem -- FPaths::ProjectDir() resolves to <archive>\Windows\Forever\ in the packaged build.
rem
rem No separate link for Forever_Mod: NTFS junctions are transparent to ".." traversal, so
rem config.json's own relative paths (e.g. "../../../Forever_Mod/Test/UE/Test") walk straight
rem through the Resource junction back into the real dev tree and find the real Forever_Mod
rem on their own -- verified by testing with that link removed. The name "Forever_Mod" (and
rem any future mod's location) therefore only needs to exist inside config.json; this script
rem never needs to know it.
if exist "%ARCHIVE%\Windows\Forever\Resource" rmdir /s /q "%ARCHIVE%\Windows\Forever\Resource"
mklink /J "%ARCHIVE%\Windows\Forever\Resource" "%~dp0Resource"
if errorlevel 1 goto :fail
if exist "%ARCHIVE%\Windows\Forever\x64" rmdir /s /q "%ARCHIVE%\Windows\Forever\x64"
mkdir "%ARCHIVE%\Windows\Forever\x64"
mklink /J "%ARCHIVE%\Windows\Forever\x64\Release" "%~dp0x64\Release"
if errorlevel 1 goto :fail

echo Done.
goto :end
:fail
echo Failed.
exit /b 1
:end
