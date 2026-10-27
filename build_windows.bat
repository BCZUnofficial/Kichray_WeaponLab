@echo off
setlocal
cd /d "%~dp0"
where msbuild >nul 2>nul
if errorlevel 1 (
  echo Open "x64 Native Tools Command Prompt for VS 2022" and run this file again.
  exit /b 1
)
msbuild Kichray_WeaponLab.sln /m /t:Build /p:Configuration=Release /p:Platform=x64
if errorlevel 1 exit /b 1
echo.
echo Built: bin\Release\x64\WeaponLab.exe
echo Keep assets\Preview.hlsl next to the executable.
