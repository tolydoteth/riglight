@echo off
setlocal
cd /d "%~dp0"
if not exist build\build.ninja (
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ || exit /b 1
)
cmake --build build || exit /b 1
ctest --test-dir build --output-on-failure || exit /b 1
if not exist build\RigLight.exe exit /b 0
tasklist /fi "imagename eq RigLight.exe" | find /i "RigLight.exe" >nul && (
  echo RigLight is running: quit it from the tray to update RigLight.exe here.
  exit /b 0
)
copy /y build\RigLight.exe RigLight.exe >nul || exit /b 1
copy /y third_party\iCUESDK\redist\x64\iCUESDK.x64_2019.dll iCUESDK.x64_2019.dll >nul || exit /b 1
echo RigLight is ready: %~dp0RigLight.exe
