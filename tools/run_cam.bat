@echo off
rem ------------------------------------------------------------------
rem 把图像推到 OLED 上的快捷入口（省得每次敲 Python 全路径）
rem
rem 双击运行           -> 列出可用串口并提示用法
rem run_cam --port COM3 --camera 1 --preview      摄像头（本机 USB 摄像头是 1）
rem run_cam --port COM3 --test                     测试图案，不需要摄像头
rem run_cam --list-cameras                         看看摄像头编号到底是几
rem
rem 如果换了 Python（比如自己装的官方版），改下面 PY 这一行即可
rem ------------------------------------------------------------------
setlocal

set "PY=C:\Users\86139\.workbuddy\binaries\python\versions\3.14.3\python.exe"

if not exist "%PY%" (
    echo 找不到 Python：%PY%
    echo 请把本文件里的 PY 改成你自己的 python.exe 路径。
    pause
    exit /b 1
)

if "%~1"=="" (
    "%PY%" "%~dp0cam_serial.py" --list
    echo.
    echo 用法示例：
    echo   run_cam --port COM3 --test
    echo   run_cam --port COM3 --camera 1 --preview --fps 20
    echo   run_cam --port COM3 --camera 1 --mode face --ascii
    echo   run_cam --list-cameras
    echo.
    pause
    exit /b 0
)

"%PY%" "%~dp0cam_serial.py" %*
endlocal
