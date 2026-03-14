cmake_minimum_required(VERSION 3.16)
project(windows_cache_scanner_qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)

add_executable(windows_cache_scanner_console
  windows_cache_scanner_example.cpp
)

target_link_libraries(windows_cache_scanner_console PRIVATE Qt${QT_VERSION_MAJOR}::Core)

add_executable(windows_cache_scanner_gui
  windows_cache_scanner_qt.cpp
  windows_cache_scan_table_model.cpp
  windows_cache_scanner_window.cpp
)

target_link_libraries(windows_cache_scanner_gui PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Widgets)
