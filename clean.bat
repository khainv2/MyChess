@echo off
echo =====================================
echo     MyChess Clean Script
echo =====================================
echo.

set PROJECT_DIR=%~dp0
set BUILD_DIR=%PROJECT_DIR%_build

echo Removing build directory...

if exist "%BUILD_DIR%" (
    rmdir /s /q "%BUILD_DIR%"
    echo ✓ Removed _build directory
) else (
    echo _build directory does not exist
)

echo.
echo Cleanup completed!
echo You can run build.bat to rebuild the project.
echo.
pause
