@echo off
setlocal
title ISHTAR installer

set "SRC=%~dp0ISHTAR.vst3"
set "DEST=C:\Program Files\Common Files\VST3\ISHTAR.vst3"

if not exist "%SRC%\Contents\x86_64-win\ISHTAR.vst3" (
    echo ERROR: The ISHTAR.vst3 folder was not found next to this script.
    echo Please extract the WHOLE zip first, then run Install ISHTAR from
    echo the extracted folder.
    echo.
    pause
    exit /b 1
)

echo Installing ISHTAR to:
echo   %DEST%
echo.

robocopy "%SRC%" "%DEST%" /MIR /R:2 /W:5 /NJH /NJS /NDL /NP >nul
if not errorlevel 8 goto :postcopy

rem Copy failed. If we are not admin yet, retry elevated; otherwise report.
net session >nul 2>&1
if not errorlevel 1 goto :copyfailed
if "%~1"=="elevated" goto :copyfailed

echo Administrator permission is needed to write to Program Files.
echo Please approve the prompt that appears...
powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -ArgumentList 'elevated' -Verb RunAs"
if errorlevel 1 (
    echo.
    echo Installation was cancelled - ISHTAR has not been installed.
    echo.
    pause
)
exit /b

:copyfailed
echo.
echo ERROR: Could not copy the plug-in. If your DAW (Cubase, etc.) is open,
echo please close it and run this installer again.
echo.
pause
exit /b 1

:postcopy
rem Give the bundle folder its ISHTAR icon in Explorer.
if exist "%DEST%\desktop.ini" attrib -s -h -r "%DEST%\desktop.ini"
> "%DEST%\desktop.ini" (
    echo [.ShellClassInfo]
    echo IconResource=Contents\Resources\ISHTAR.ico,0
    echo ConfirmFileOp=0
)
attrib +s +h "%DEST%\desktop.ini"
attrib +r "%DEST%"

echo.
echo ISHTAR was installed successfully.
echo If your DAW was open, restart it and rescan plug-ins.
echo.
pause
exit /b 0
