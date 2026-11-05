@echo off
rem Build ModHost (Release). Requires Dependence.lib and Basic.dll in ..\..\x64\Release.
set MSBUILD="C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
%MSBUILD% "%~dp0ModHost.vcxproj" /p:Configuration=Release /p:Platform=x64 /m /v:minimal %*
