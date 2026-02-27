@echo off
setlocal EnableDelayedExpansion

echo ==========================================
echo Cleaning Unreal Project...
echo ==========================================

REM Project root = folder where this bat file is located
set PROJECT_DIR=%~dp0
cd /d "%PROJECT_DIR%"

REM Folders to delete
for %%F in (.vs Binaries DerivedDataCache Intermediate Saved) do (
    if exist "%%F" (
        echo Deleting folder %%F ...
        rmdir /s /q "%%F"
    ) else (
        echo Folder %%F not found, skipping...
    )
)

echo.
echo ==========================================
echo Deleting solution/config/user files...
echo ==========================================

REM Delete any .sln in the root (typical UE output)
for %%S in (*.sln) do (
    if exist "%%S" (
        echo Deleting file %%S ...
        del /f /q "%%S"
    )
)

REM Delete the specific JetBrains/Rider user settings file if present
if exist "Myceland.sln.DotSettings.user" (
    echo Deleting file Myceland.sln.DotSettings.user ...
    del /f /q "Myceland.sln.DotSettings.user"
)

REM Also delete any .DotSettings.user (covers renamed projects/other solutions)
for %%D in (*.DotSettings.user) do (
    if exist "%%D" (
        echo Deleting file %%D ...
        del /f /q "%%D"
    )
)

REM Delete Visual Studio config files
for %%V in (*.vsconfig) do (
    if exist "%%V" (
        echo Deleting file %%V ...
        del /f /q "%%V"
    )
)

echo.
echo ==========================================
echo Searching for .uproject file...
echo ==========================================

set UPROJECT=
for %%P in (*.uproject) do (
    set UPROJECT=%%P
)

if not defined UPROJECT (
    echo ERROR: No .uproject file found in "%PROJECT_DIR%" !
    pause
    exit /b 1
)

echo Found: %UPROJECT%
echo.

REM Small pause so filesystem catches up (especially on network drives/OneDrive)
echo Waiting a moment...
timeout /t 2 /nobreak >nul

echo ==========================================
echo Regenerating Visual Studio solution...
echo ==========================================

REM First try UnrealVersionSelector (works for most launcher installs)
set UVS="%ProgramFiles(x86)%\Epic Games\Launcher\Engine\Binaries\Win64\UnrealVersionSelector.exe"

if exist %UVS% (
    %UVS% /projectfiles "%PROJECT_DIR%%UPROJECT%"
    if %errorlevel% equ 0 goto done
)

echo UnrealVersionSelector failed or not found. Trying fallback...

REM Fallback: try GenerateProjectFiles.bat from any UE_* install folder
for /d %%E in ("C:\Program Files\Epic Games\UE_*") do (
    if exist "%%E\Engine\Build\BatchFiles\GenerateProjectFiles.bat" (
        call "%%E\Engine\Build\BatchFiles\GenerateProjectFiles.bat" -project="%PROJECT_DIR%%UPROJECT%" -game -engine
        goto done
    )
)

echo ERROR: Could not regenerate project files (no UE install found in default location).
echo You may need to edit the script to point to your UE installation.
pause
exit /b 2

:done
echo.
echo ==========================================
echo DONE!
echo ==========================================
pause