@echo off
rem Build + stage a runnable folder on Windows. Needs CMake and either Visual Studio Build Tools or MinGW-w64.
rem Output: out\Launch.exe with ui\ and data\ beside it (static runtime: no redistributable needed).
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build build --config Release || exit /b 1
if exist build\Release\Launch.exe (set BIN=build\Release) else (set BIN=build)
if exist out rmdir /s /q out
mkdir out
xcopy /e /i /y %BIN%\ui out\ui >nul
xcopy /e /i /y %BIN%\data out\data >nul
copy /y %BIN%\Launch.exe out\Launch.exe >nul
echo Done: out\Launch.exe  (copy it, ui\ and data\ to the ZIP root)
