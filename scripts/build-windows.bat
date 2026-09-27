@echo off
setlocal
rem ---------------------------------------------------------------------------
rem  sonic1_pc - Windows build helper (runs from cmd.exe, compiles inside WSL2)
rem
rem  sonic1_pc is built with mingw-w64 from Linux, so on Windows the build is
rem  driven through WSL2. All arguments are forwarded to build-windows.sh:
rem
rem    build-windows.bat                       -> release
rem    build-windows.bat debug --clean
rem    build-windows.bat all
rem    build-windows.bat release --prefix \\wsl$\Ubuntu\home\me\mingw64
rem
rem  Result: dist\win\<variant>\sonic1.exe (ready to copy to Windows).
rem ---------------------------------------------------------------------------

set "RAW_SCRIPT_DIR=%~dp0scripts"
set "RAW_SCRIPT_DIR=%RAW_SCRIPT_DIR:\=/%"

rem Use the distro this script was launched from, when cmd.exe knows it.
set "WSL_DISTRO_ARG="
if defined WSL_DISTRO_NAME set "WSL_DISTRO_ARG=-d %WSL_DISTRO_NAME%"

set "WSL_SCRIPT_DIR="
for /f "usebackq delims=" %%i in (`wsl.exe %WSL_DISTRO_ARG% wslpath -a "%RAW_SCRIPT_DIR%" 2^>nul`) do set "WSL_SCRIPT_DIR=%%i"

if not defined WSL_SCRIPT_DIR (
    echo [error] Could not reach WSL2 from this shell.
    echo         Install WSL from the Microsoft Store, then run this script again
    echo         from inside a WSL distro ^(wsl.exe -d Ubuntu bash^).
    exit /b 1
)

wsl.exe %WSL_DISTRO_ARG% bash -lc "cd '%WSL_SCRIPT_DIR%' && ./build-windows.sh %*"
if errorlevel 1 (
    echo.
    echo [error] The WSL build failed, see the log above.
    exit /b 1
)

echo.
echo [ok] Done. Copy the dist\win\^\<variant^> folder to Windows and run sonic1.exe.
endlocal
