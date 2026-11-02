# Unified Engineering Workbench (Pass Science Works R&D assignment)

C++17 backend + browser-based UI served from a local loopback port. Contains: STL / nodal-CSV viewer (Task B), 1-D thermal FEM (C),
multithreaded batch solver with benchmark (D), simulated embedded devices (E), hand-derived backprop check (F).

## One-click use (Windows 11 x64)
1. Extract the ZIP anywhere (no admin rights). 2. Double-click **Launch.exe**. The default browser opens the Workbench at `http://127.0.0.1:8765/`.
No terminal, PowerShell, package install, internet or build step is needed (static runtime, no third-party DLLs). The app listens on loopback only.
If port 8765 is busy it uses the next free one up to 8785. Close the browser tab and end `Launch.exe` in Task Manager to quit.
**Editor:** open the folder in VS Code, install the "JavaScript Debug Terminal" support (built in) and choose **Run and Debug -> "Workbench: run packaged Launch.exe"**.
The standalone launcher does not need the editor.

## Prerequisites to *build* (not to run)
CMake >= 3.20 and MSVC Build Tools 2019+ or MinGW-w64 (GCC >= 9) or any C++17 compiler on Linux/macOS. No third-party libraries (see DEPENDENCIES.md).

## Build / test
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
    ctest --test-dir build            # or run wb_tests directly: 104 checks
    tools\build_windows.bat           # Windows: stages out\Launch.exe + ui\ + data\
Evidence generators: `wb_cli fea|backprop|bench results [cases n reps] [--force-workers]`, then `python tools/make_plots.py` (plotting helper only).

## Architecture
    src/core/      pure C++17 (no UI/IO transport): fea, batch (worker pool), formats (STL/CSV), backprop, devices (mock), sysinfo
    src/app/       transport + presentation glue: minimal HTTP server (http.*), JSON helpers, routes, job manager, browser launch
    src/frontend/  single-file HTML/JS UI (Geometry, Thermal Analysis, Performance, Hardware, Backprop views); no computation of results
    tests/ tools/ data/ results/ docs/
All parsing, FEA, parallel work, gradients and device logic run in C++; the JS only renders. Long jobs (benchmark) run on a C++ thread; the UI polls
progress and can cancel; cancelled runs discard partial data.

## Supported formats and limits
* ASCII STL (`solid`/`facet`/`vertex`, units assumed metres). Binary STL, NaN/Inf, truncated facets and empty files are rejected with line numbers.
* CSV header exactly `node_id,x_m,y_m,z_m,temperature_C` (m, deg C); duplicate ids, wrong columns, non-finite values rejected. Examples: `data/` (+ PROVENANCE.md; the CSV is SYNTHETIC).
* Hard cap 100 MB per file. STL view draws at most ~60k triangles (decimated beyond that). Tested sizes: see docs/REPORT.md section 6.

## Known limitations
See docs/REPORT.md section 7 (headline: Launch.exe must be built on Windows with `tools/build_windows.bat`; speedup measured only on a 1-CPU VM; UI not
yet checked in multiple browsers; hardware is simulated; handwritten backprop pages, video and real Git timeline are for the candidate to produce).
