@echo off
setlocal EnableDelayedExpansion

echo =====================================
echo     MyChess Console Build Script
echo =====================================
echo.

REM Setup environment variables
set QT_PATH=C:\Qt\Qt5.12.12\5.12.12
set PROJECT_DIR=%~dp0
set BUILD_DIR=%PROJECT_DIR%_build_console

REM Check Qt installation
echo [1/6] Checking Qt installation...
if not exist "%QT_PATH%" (
    echo ERROR: Qt not found at %QT_PATH%
    pause
    exit /b 1
)

REM Setup MSVC 2017 compiler
echo [2/6] Setting up MSVC 2017 compiler...
if exist "%QT_PATH%\msvc2017_64" (
    set QT_BIN_PATH=%QT_PATH%\msvc2017_64\bin
    set MAKE_CMD=nmake
    echo Found: MSVC 2017 64-bit
) else (
    echo ERROR: MSVC 2017 64-bit not found
    pause
    exit /b 1
)

REM Setup Visual Studio 2017 environment
echo [2.1/6] Setting up Visual Studio 2017 environment...
set VS2017_PATHS[0]="C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
set VS2017_PATHS[1]="C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvars64.bat"
set VS2017_PATHS[2]="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"
set VS2017_PATHS[3]="C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

set VCVARS_FOUND=0
for /l %%i in (0,1,3) do (
    if exist !VS2017_PATHS[%%i]! (
        echo Setting up Visual Studio 2017 environment...
        call !VS2017_PATHS[%%i]! >nul 2>&1
        set VCVARS_FOUND=1
        goto :vcvars_done
    )
)

:vcvars_done
if !VCVARS_FOUND! == 0 (
    echo WARNING: Visual Studio 2017 not found in standard locations
    echo Trying to use system PATH for MSVC tools...
)
echo.

REM Create build directory
echo [3/6] Preparing build directory...
if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%"
    echo Created _build_console directory
) else (
    echo _build_console directory already exists
)

cd /d "%BUILD_DIR%"

REM Run qmake
echo [4/6] Creating Makefile with qmake...
"%QT_BIN_PATH%\qmake.exe" "..\MyChessConsole.pro"
if errorlevel 1 (
    echo ERROR: qmake failed
    pause
    exit /b 1
)
echo qmake successful!

REM Build
echo [5/6] Building...
if /i "%~1"=="release" (
    echo Building Release...
    !MAKE_CMD! release
    set EXE_DIR=release
) else (
    echo Building Debug...
    !MAKE_CMD! debug
    set EXE_DIR=debug
)
if errorlevel 1 (
    echo ERROR: Build failed
    pause
    exit /b 1
)
echo.
echo Build successful!

REM Run
echo [6/6] Running MyChessConsole...
echo =====================================
echo.

REM Add Qt DLLs to PATH
set PATH=!QT_BIN_PATH!;%PATH%

REM Change back to project directory so relative paths work
cd /d "%PROJECT_DIR%"

set EXTRA_ARGS=
if not "%~2"=="" (
    set EXTRA_ARGS=--limit %~2
)

"%BUILD_DIR%\!EXE_DIR!\MyChessConsole.exe" --epd "tests\wac.epd" %EXTRA_ARGS%

echo.
echo =====================================
pause
