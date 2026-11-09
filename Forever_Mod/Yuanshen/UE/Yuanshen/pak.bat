@echo off
set ENGINE=C:\Workspace\UE5\Engine\UE_5.7
for %%I in ("%~dp0.") do set "NAME=%%~nI"
set PROJECT=%~dp0%NAME%.uproject
set COOKED_OUT=%~dp0Saved\Cooked\Windows\%NAME%\Plugins\%NAME%\Content
set COOKED_SHADERS=%~dp0Saved\Cooked\Windows\%NAME%\Content
set PAK_DIR=%~dp0Plugins\%NAME%_Pak

if not exist "%PAK_DIR%" mkdir "%PAK_DIR%"
if exist "%PAK_DIR%\%NAME%.pak" del "%PAK_DIR%\%NAME%.pak"
if exist "%~dp0Saved\Cooked" rmdir /s /q "%~dp0Saved\Cooked"

"%ENGINE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT%" ^
  -run=Cook -TargetPlatform=Windows -unversioned -CookAll -stdout -unattended
if errorlevel 1 goto :fail

powershell -Command "$src='%COOKED_OUT%'; $shaders='%COOKED_SHADERS%'; $lines = Get-ChildItem -Recurse -File $src | ForEach-Object { $rel=$_.FullName.Substring($src.Length+1).Replace('\','/'); '\"'+$_.FullName+'\" \"../../../%NAME%/Content/'+$rel+'\"' }; $shaderLines = Get-ChildItem -File $shaders -Filter 'ShaderArchive-%NAME%-*.ushaderbytecode' | ForEach-Object { '\"'+$_.FullName+'\" \"../../../%NAME%/Content/'+$_.Name+'\"' }; ($lines + $shaderLines) | Set-Content '%PAK_DIR%\filelist.txt' -Encoding ASCII"
if errorlevel 1 goto :fail

"%ENGINE%\Engine\Binaries\Win64\UnrealPak.exe" "%PAK_DIR%\%NAME%.pak" -Create="%PAK_DIR%\filelist.txt" -compress
if errorlevel 1 goto :fail

del "%PAK_DIR%\filelist.txt"
echo Done.
goto :end

:fail
echo Failed.
exit /b 1

:end
