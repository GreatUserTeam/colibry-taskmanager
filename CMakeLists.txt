cmake_minimum_required(VERSION 3.21)

project(colibry-taskmanager
    VERSION 0.2.0
    DESCRIPTION "Tasks for the Colibry"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(Colibry_BUILD_TESTS "Build unit tests" ON)
option(Colibry_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(_qt_components Widgets)
if(Colibry_BUILD_TESTS)
    list(APPEND _qt_components Test)
endif()
find_package(Qt6 6.4 REQUIRED COMPONENTS ${_qt_components})

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(ColibryApps)

if(Colibry_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(common)
add_library(gus_tm_core STATIC
    src/core/ProcessData.h
    src/core/ProcessProvider.h
    src/core/ProcFsProvider.h
    src/core/ProcFsProvider.cpp
    src/core/ProcessMonitor.h
    src/core/ProcessMonitor.cpp
    src/core/ProcessModel.h
    src/core/ProcessModel.cpp
    src/core/Format.h
    src/core/Format.cpp
)
target_include_directories(gus_tm_core PUBLIC src)
target_link_libraries(gus_tm_core PUBLIC Qt6::Widgets)
colibry_target_defaults(gus_tm_core)

add_library(gus_tm_ui STATIC
    src/HistoryGraph.h
    src/HistoryGraph.cpp
    src/TaskWindow.h
    src/TaskWindow.cpp
)
target_include_directories(gus_tm_ui PUBLIC src)
target_link_libraries(gus_tm_ui PUBLIC gus_tm_core colibry_common Qt6::Widgets)
colibry_target_defaults(gus_tm_ui)

add_executable(colibry-tasks src/main.cpp)
target_link_libraries(colibry-tasks PRIVATE gus_tm_ui)
colibry_target_defaults(colibry-tasks)

colibry_install_app(colibry-tasks
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging/colibry-tasks.desktop
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging/colibry-tasks.svg)

if(Colibry_BUILD_TESTS)
    colibry_add_test(tm_core SOURCES tests/test_core.cpp LINK gus_tm_core)
    colibry_add_test(tm_ui SOURCES tests/test_ui.cpp LINK gus_tm_ui)
endif()
