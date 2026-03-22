@echo off
setlocal enabledelayedexpansion

:: ============================================================================
:: GenerateProjectFiles.bat
:: Generates Visual Studio project files for FactoryGame (the CSS Satisfactory
:: modding project). Looks up the CSS custom Unreal Engine 5.3.2-CSS from the
:: Windows registry where SetupScripts\Register.bat registers it.
::
:: Usage:
::   GenerateProjectFiles.bat            -- standard project files
::   GenerateProjectFiles.bat -dotnet    -- also include Alpakit C# projects
:: ============================================================================

set "UE_REG_KEY=HKCU\Software\Epic Games\Unreal Engine\Builds"
set "UE_VERSION=5.3.2-CSS"
set "PROJECT_FILE=%~dp0FactoryGame.uproject"
set "EXTRA_ARGS=%*"

:: Look up the engine installation path from the registry
set "ENGINE_DIR="
for /f "tokens=2*" %%a in ('reg query "%UE_REG_KEY%" /v "%UE_VERSION%" 2^>nul') do (
    set "ENGINE_DIR=%%b"
)

if not defined ENGINE_DIR (
    echo.
    echo ERROR: Unreal Engine build '%UE_VERSION%' was not found in the registry.
    echo.
    echo Please ensure the CSS custom Unreal Engine is installed and registered:
    echo   1. Download UnrealEngine-CSS from the satisfactorymodding/UnrealEngine
    echo      releases on GitHub.
    echo   2. Extract the archive.
    echo   3. Run SetupScripts\Register.bat inside the extracted engine folder.
    echo   4. Re-run this script.
    echo.
    pause
    exit /b 1
)

set "UBT=%ENGINE_DIR%\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe"

if not exist "%UBT%" (
    echo.
    echo ERROR: UnrealBuildTool.exe not found at:
    echo   %UBT%
    echo.
    echo The engine registration at '%UE_VERSION%' may point to an incomplete or
    echo moved installation. Re-run SetupScripts\Register.bat from the engine folder.
    echo.
    pause
    exit /b 1
)

echo Generating project files using CSS UE %UE_VERSION%...
echo Engine: %ENGINE_DIR%
echo.

"%UBT%" -projectfiles -project="%PROJECT_FILE%" -game -rocket -progress %EXTRA_ARGS%

if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Project file generation failed (exit code %ERRORLEVEL%).
    echo.
    echo Common causes:
    echo   * Wwise is not installed. Download and extract Wwise into Plugins\
    echo     before generating project files.
    echo   * A module Build.cs contains a syntax error.
    echo.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Project files generated successfully.
echo Open FactoryGame.sln in Visual Studio or Rider to begin development.
echo.
echo TIP: To also include Alpakit C# project files, run:
echo   GenerateProjectFiles.bat -dotnet
echo.
