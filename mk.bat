@echo off
chcp 65001 >nul
setlocal
set "PATH=C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\mingw1310_64\bin;%PATH%"
cd /d "d:\gongzuo\zuoye\huochairen\cpp_code"
if not exist "build_local" mkdir build_local
cd build_local
cmake -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.7.3/mingw_64" -DCMAKE_BUILD_TYPE=Release .. > cmake_cfg.log 2>&1
if errorlevel 1 (
  echo [CFG FAILED]
  type cmake_cfg.log
  exit /b 1
)
cmake --build . -j 8 > build.log 2>&1
if errorlevel 1 (
  echo [BUILD FAILED]
  type build.log
  exit /b 1
)
echo [BUILD OK]
dir /b MultiMediaPlayer.exe
