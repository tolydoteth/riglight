@echo off
setlocal
cd /d "%~dp0"
rem Corsair's iCUE SDK (headers + DLL) is not redistributable here: fetch the official release once.
if not exist third_party\iCUESDK\include\iCUESDK\iCUESDK.h (
  echo Downloading Corsair iCUE SDK 4.0.84 from github.com/CorsairOfficial/cue-sdk ...
  powershell -NoProfile -Command "$ErrorActionPreference='Stop'; $z=Join-Path $env:TEMP 'iCUESDK_4.0.84.zip'; Invoke-WebRequest 'https://github.com/CorsairOfficial/cue-sdk/releases/download/v4.0.84/iCUESDK_4.0.84.zip' -OutFile $z; Expand-Archive -Force $z third_party; Remove-Item $z" || exit /b 1
)
if not exist build\build.ninja (
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ || exit /b 1
)
cmake --build build || exit /b 1
ctest --test-dir build --output-on-failure || exit /b 1
if not exist build\RigLight.exe exit /b 0
tasklist /fi "imagename eq RigLight.exe" | "%SystemRoot%\System32\find.exe" /i "RigLight.exe" >nul && (
  echo RigLight is running: quit it from the tray to update RigLight.exe here.
  exit /b 0
)
copy /y build\RigLight.exe RigLight.exe >nul || exit /b 1
copy /y third_party\iCUESDK\redist\x64\iCUESDK.x64_2019.dll iCUESDK.x64_2019.dll >nul || exit /b 1
echo RigLight is ready: %~dp0RigLight.exe
