@echo off
setlocal

set "UE_BUILD=C:\Workspace\UE5\Engine\UE_5.7\Engine\Build\BatchFiles\Build.bat"
set "PROJECT=C:\Workspace\UE5\Projects\Forever\Forever_UE\Forever.uproject"
set "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
set "MOD_ROOT=C:\Workspace\UE5\Projects\Forever\Forever_Mod"

echo ==== Building Framework.sln (Dependence/Core/Basic) ====
"%MSBUILD%" "C:\Workspace\UE5\Projects\Forever\Forever_UE\Source\Framework.sln" /p:Configuration=Release /p:Platform=x64 /m
if errorlevel 1 goto :fail

echo ==== Building ForeverEditor (UE module) ====
call "%UE_BUILD%" ForeverEditor Win64 Development -Project="%PROJECT%" -WaitMutex -FromMsBuild
if errorlevel 1 goto :fail

echo ==== Building Forever_Mod\Empty ====
"%MSBUILD%" "%MOD_ROOT%\Empty\Cpp\Empty.sln" /p:Configuration=Release /p:Platform=x64 /m
if errorlevel 1 goto :fail

echo ==== Building Forever_Mod\Test ====
"%MSBUILD%" "%MOD_ROOT%\Test\Cpp\Test.sln" /p:Configuration=Release /p:Platform=x64 /m
if errorlevel 1 goto :fail

echo ==== Building Forever_Mod\Wxdj ====
"%MSBUILD%" "%MOD_ROOT%\Wxdj\Cpp\Wxdj.sln" /p:Configuration=Release /p:Platform=x64 /m
if errorlevel 1 goto :fail

echo ==== All builds succeeded ====
exit /b 0

:fail
echo ==== BUILD FAILED ====
exit /b 1
