@echo off
rem Launcher for ISHTAR Resynth. Requires Python 3 with numpy, scipy,
rem matplotlib and soundfile (py -m pip install numpy scipy matplotlib soundfile)
cd /d "%~dp0"
py ishtar_resynth.py %*
if errorlevel 1 pause
