cmake_minimum_required(VERSION 3.10)
project(user_mode_scheduler)
# Usage:
# Create initial cmake build folder: cmake -S . -B .\build -G "Unix Makefiles"
# Create build with desired config: cmake --build .\build --config [Debug/Release/RelWithDebInfo/MinSizeRel] --parallel 8
#
# Clean command: cmake --build .\build --target clean

# Below is not working for some reason.
# set(CMAKE_GENERATOR "Unix Makefiles")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # -> compile_commands.json

# Separate bin directory from build directory.
#
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)

add_executable(user_mode_scheduler main.cpp cpu.cpp os_specific.cpp scheduler.cpp task_manager.cpp worker.cpp)
target_include_directories(user_mode_scheduler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# For previous version created with VS (not VS code), we needed UNICODE definitions because all APIs used wchar type.
#
# add_definitions(-DUNICODE)

# This if(MSVC) need to be put after project(dumper) in order to work.
#
SET(CMAKE_CXX_FLAGS  "-Wall")
if(MSVC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
endif()

# Adding dbghelp.lib for windows in order to use MiniDumpWriteDump function.
#
# if(WIN32)
#     target_link_libraries(dumper dbghelp)
# endif()

# Separate debug from release build.
#
# set(CMAKE_DEBUG_POSTFIX Debug)
# set_target_properties(dumper PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
