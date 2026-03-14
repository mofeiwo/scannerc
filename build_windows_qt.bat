@echo off
setlocal

set SCRIPT_DIR=%~dp0
set BUILD_DIR=%SCRIPT_DIR%build-win
set RULE_FILE=%SCRIPT_DIR%windows_cache_rules.json

if "%QT_ROOT%"=="" (
  echo [ERROR] QT_ROOT is not set.
  echo Example:
  echo   set QT_ROOT=C:\Qt\6.8.2\msvc2022_64
  exit /b 1
)

if not exist "%QT_ROOT%\bin\qmake.exe" (
  echo [ERROR] qmake.exe not found under %QT_ROOT%\bin
  exit /b 1
)

where cmake >nul 2>nul
if errorlevel 1 (
  echo [ERROR] cmake not found in PATH.
  exit /b 1
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
pushd "%BUILD_DIR%"

cmake -G "Ninja" ^
  -DCMAKE_PREFIX_PATH="%QT_ROOT%" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -S "%SCRIPT_DIR%" ^
  -B "%BUILD_DIR%"

if errorlevel 1 (
  echo [ERROR] CMake configure failed.
  popd
  exit /b 1
)

cmake --build "%BUILD_DIR%" --config Release --target windows_cache_scanner_gui
if errorlevel 1 (
  echo [ERROR] Build failed.
  popd
  exit /b 1
)

set EXE_PATH=%BUILD_DIR%\windows_cache_scanner_gui.exe
if not exist "%EXE_PATH%" (
  if exist "%BUILD_DIR%\Release\windows_cache_scanner_gui.exe" (
    set EXE_PATH=%BUILD_DIR%\Release\windows_cache_scanner_gui.exe
  )
)

if not exist "%EXE_PATH%" (
  echo [ERROR] Built exe not found.
  popd
  exit /b 1
)

copy /Y "%RULE_FILE%" "%~dp0windows_cache_rules.json" >nul
copy /Y "%RULE_FILE%" "%BUILD_DIR%\windows_cache_rules.json" >nul

"%QT_ROOT%\bin\windeployqt.exe" "%EXE_PATH%"
if errorlevel 1 (
  echo [ERROR] windeployqt failed.
  popd
  exit /b 1
)

echo.
echo [OK] Build completed:
echo   %EXE_PATH%
echo [OK] Rules file copied:
echo   %BUILD_DIR%\windows_cache_rules.json

popd
endlocal
