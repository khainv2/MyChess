@echo off
setlocal EnableDelayedExpansion

echo =====================================
echo     MyChess Build Script
echo =====================================
echo.

REM Setup environment variables
set QT_PATH=C:\Qt\Qt5.12.12\5.12.12
set PROJECT_DIR=%~dp0
set BUILD_DIR=%PROJECT_DIR%_build

REM Check Qt installation
echo [1/6] Checking Qt installation...
if not exist "%QT_PATH%" (
    echo ERROR: Qt not found at %QT_PATH%
    echo Please check your Qt installation path
    pause
    exit /b 1
)

REM Setup MSVC 2017 compiler
echo [2/6] Setting up MSVC 2017 compiler...
if exist "%QT_PATH%\msvc2017_64" (
    set QT_BIN_PATH=%QT_PATH%\msvc2017_64\bin
    set COMPILER_TYPE=msvc2017_64
    set MAKE_CMD=nmake
    echo Found: MSVC 2017 64-bit
) else (
    echo ERROR: MSVC 2017 64-bit not found at %QT_PATH%\msvc2017_64
    echo Please ensure Qt 5.12.12 with MSVC 2017 64-bit is installed
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
    echo If build fails, please install Visual Studio 2017 or ensure MSVC is in PATH
)

echo Using compiler: !COMPILER_TYPE!
echo.

REM Create build directory
echo [3/6] Preparing build directory...
if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%"
    echo Created _build directory
) else (
    echo _build directory already exists
)

REM Change to build directory
cd /d "%BUILD_DIR%"

REM Run qmake
echo [4/6] Creating Makefile with qmake...
"%QT_BIN_PATH%\qmake.exe" "..\MyChess.pro"
if errorlevel 1 (
    echo ERROR: qmake failed
    pause
    exit /b 1
)
echo qmake successful!

REM Check for command line parameter
if "%~1"=="" (
    set BUILD_CHOICE=1
    echo [5/6] No parameter specified, defaulting to Debug build
) else if /i "%~1"=="debug" (
    set BUILD_CHOICE=1
    echo [5/6] Building Debug (from parameter)
) else if /i "%~1"=="release" (
    set BUILD_CHOICE=2
    echo [5/6] Building Release (from parameter)
) else if /i "%~1"=="both" (
    set BUILD_CHOICE=3
    echo [5/6] Building both Debug and Release (from parameter)
) else (
    echo [5/6] Invalid parameter '%~1'. Valid options: debug, release, both
    echo Defaulting to Debug build
    set BUILD_CHOICE=1
)

REM Execute build based on choice
echo.
echo [6/6] Starting build...

if "%BUILD_CHOICE%"=="1" (
    echo Building Debug...
    !MAKE_CMD! debug
    if errorlevel 1 (
        echo ERROR: Debug build failed
        pause
        exit /b 1
    )
    echo.
    echo ✓ Debug build successful!
    echo Executable: %BUILD_DIR%\debug\MyChess.exe
) else if "%BUILD_CHOICE%"=="2" (
    echo Building Release...
    !MAKE_CMD! release
    if errorlevel 1 (
        echo ERROR: Release build failed
        pause
        exit /b 1
    )
    echo.
    echo ✓ Release build successful!
    echo Executable: %BUILD_DIR%\release\MyChess.exe
) else if "%BUILD_CHOICE%"=="3" (
    echo Building both Debug and Release...
    !MAKE_CMD!
    if errorlevel 1 (
        echo ERROR: Build failed
        pause
        exit /b 1
    )
    echo.
    echo ✓ Build successful!
    echo Debug executable: %BUILD_DIR%\debug\MyChess.exe
    echo Release executable: %BUILD_DIR%\release\MyChess.exe
) else (
    echo Invalid choice. Building both...
    !MAKE_CMD!
    if errorlevel 1 (
        echo ERROR: Build failed
        pause
        exit /b 1
    )
    echo.
    echo ✓ Build successful!
    echo Debug executable: %BUILD_DIR%\debug\MyChess.exe
    echo Release executable: %BUILD_DIR%\release\MyChess.exe
)

echo.
echo =====================================
echo Build completed!
echo =====================================
echo.

REM Ask if user wants to run the application
set /p RUN_APP="Do you want to run the application? (y/n): "
if /i "%RUN_APP%"=="y" (
    if exist "release\MyChess.exe" (
        echo Running Release version...
        start "MyChess" "release\MyChess.exe"
    ) else if exist "debug\MyChess.exe" (
        echo Running Debug version...
        start "MyChess" "debug\MyChess.exe"
    ) else (
        echo Executable not found
    )
)

REM Return to project directory
cd /d "%PROJECT_DIR%"

echo Thank you for using MyChess Build Script!
pause
