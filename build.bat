@echo off
chcp 65001 >nul
setlocal

rem ============================================================
rem MultiMediaPlayer 一键构建脚本
rem 用法: build.bat [Qt安装路径]
rem 示例: build.bat C:\Qt\6.7.0\msvc2022_64
rem 或先设置环境变量 QT_DIR=C:\Qt\6.7.0\msvc2022_64 再 build.bat
rem ============================================================

set "QT_DIR=%~1"
if "%QT_DIR%"=="" set "QT_DIR=%QT_DIR%"

if "%QT_DIR%"=="" (
    echo [错误] 未指定 Qt 路径!
    echo.
    echo 用法: build.bat ^<Qt安装路径^>
    echo 示例: build.bat C:\Qt\6.7.0\msvc2022_64
    echo.
    echo 安装步骤:
    echo   1. 下载 Qt Online Installer: https://www.qt.io/download-qt-installer
    echo   2. 安装时勾选 Qt 6.7 (或 6.6+) 和 MSVC 2022 / MinGW 13.1 工具链
    echo   3. 确保安装了 CMake 和 Ninja
    goto :eof
)

if not exist "%QT_DIR%\bin\qmake.exe" (
    echo [错误] 在 %QT_DIR% 未找到 qmake.exe
    echo 请确认路径指向 Qt 的编译器版本目录
    goto :eof
)

echo [信息] Qt 路径: %QT_DIR%

set "PATH=%QT_DIR%\bin;%PATH%"

cd /d "%~dp0"

if exist "build" rd /s /q build
mkdir build

echo [1/3] CMake 配置...
cd build
cmake -DCMAKE_PREFIX_PATH="%QT_DIR%" -DCMAKE_BUILD_TYPE=Release ..
if errorlevel 1 (
    echo [错误] CMake 配置失败
    cd ..
    goto :eof
)

echo [2/3] 编译...
cmake --build . --config Release -j %NUMBER_OF_PROCESSORS%
if errorlevel 1 (
    echo [错误] 编译失败
    cd ..
    goto :eof
)

echo [3/3] 部署 Qt DLL...
set "EXE_DIR=%~dp0build\Release"
if not exist "%EXE_DIR%" set "EXE_DIR=%~dp0build"
if exist "%EXE_DIR%\MultiMediaPlayer.exe" (
    windeployqt "%EXE_DIR%\MultiMediaPlayer.exe"
    echo.
    echo [完成] 可执行文件: %EXE_DIR%\MultiMediaPlayer.exe
    echo 可以直接把整个 %EXE_DIR% 目录拷贝到其他电脑运行
) else (
    echo [警告] 没找到编译好的 exe, 请手动检查 build 目录
)

cd ..
endlocal
pause