cmake_minimum_required(VERSION 3.20)
project(UnifiedEngineeringWorkbench LANGUAGES CXX VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()
find_package(Threads REQUIRED)

# --- C++ core: no UI, no transport, no third-party dependencies ---
add_library(wbcore STATIC src/core/fea.cpp src/core/batch.cpp src/core/formats.cpp
            src/core/backprop.cpp src/core/devices.cpp src/core/sysinfo.cpp)
target_include_directories(wbcore PUBLIC src)
target_link_libraries(wbcore PUBLIC Threads::Threads)
if(MSVC)
  target_compile_options(wbcore PRIVATE /W4 /O2)
  target_compile_definitions(wbcore PRIVATE WB_FLAGS="/O2 /std:c++17")
else()
  target_compile_options(wbcore PRIVATE -Wall -Wextra -O2)
  target_compile_definitions(wbcore PRIVATE WB_FLAGS="-O2 -std=c++17")
endif()
if(WIN32)
  target_link_libraries(wbcore PUBLIC advapi32)
endif()

# --- presentation/transport: loopback HTTP + JSON, serves ui/ ---
add_executable(Launch src/app/main.cpp src/app/http.cpp)
target_link_libraries(Launch PRIVATE wbcore)
if(WIN32)
  target_link_libraries(Launch PRIVATE ws2_32 shell32 user32)
  target_compile_definitions(Launch PRIVATE WB_GUI)           # WinMain: no console window on double-click
  set_target_properties(Launch PROPERTIES WIN32_EXECUTABLE TRUE)
  if(MSVC)
    set_property(TARGET Launch wbcore PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")  # static CRT: no VC++ redistributable needed
  else()
    target_link_options(Launch PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
endif()
# Stage the runnable folder: Launch.exe + ui/ + data/
add_custom_command(TARGET Launch POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/frontend $<TARGET_FILE_DIR:Launch>/ui
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:Launch>/data)

add_executable(wb_cli tools/wb_cli.cpp)
target_link_libraries(wb_cli PRIVATE wbcore)
enable_testing()
add_executable(wb_tests tests/test_core.cpp)
target_link_libraries(wb_tests PRIVATE wbcore)
add_test(NAME core COMMAND wb_tests)
