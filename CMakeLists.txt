# Usage:
# Create initial cmake build folder: cmake [-G Ninja, only allowed on windows] -B build -DCMAKE_BUILD_TYPE=[Debug/Release] -DCMAKE_CXX_COMPILER=clang++-20 -DCMAKE_C_COMPILER=clang-20
# Create build with desired config: cmake --build build
# On linux, you have to use makefiles, since lib_io_uring is build that way, I am not patient enough to make it build with ninja.
#
# Clean command: cmake --build build --target clean

# If you want to play around with scheduler, you can just enable BUILD_MAIN which builds main.cpp file.

# To enable tracy profiler, build project with ENABLE_TRACY=ON and make sure to have debug symbols (e.g. RelWithDebugInfo)
# You will also need tracy server, GUI etc. installed, since we are not build it from scratch (I tried couple of times and it failed, so I gave up).
# For windows, found needed binaries under releases https://github.com/wolfpld/tracy. Note that server (downloaded binaries) and client version (one compiled by UMS) needs to be the same.
# Also, on windows, make sure to run program as administrator so tracy can collect stack traces.

cmake_minimum_required(VERSION 3.10)
project(user_mode_scheduler)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # -> compile_commands.json

# Separate bin directory from build directory.
#
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)

include(cmake/project_config.cmake)
include(cmake/sanitizers.cmake)
include(cmake/compiler_options.cmake)

set(CPP_FILES os_specific.cpp task.cpp scheduler.cpp async.cpp worker.cpp io.cpp util.cpp spinlock.cpp mutex.cpp shared_mutex.cpp condition_variable.cpp ums.cpp)
set(HPP_FILES os_specific.h task.h scheduler.h async.h worker.h io_api.h  util.h config.h spinlock.h mutex.h shared_mutex.h condition_variable.h ums.h options.h)
set(ALL_FILES ${CPP_FILES} ${HPP_FILES})

add_library(ums_lib STATIC ${CPP_FILES})
set(UMS_LIB ums_lib)

if (LINUX)
    add_subdirectory(third_party)
    target_link_libraries(ums_lib PRIVATE uring)
    add_dependencies(ums_lib uring_project)
endif()

include(cmake/profilers.cmake)
if (UMS_ENABLE_TRACY)
    add_tracy(ums_lib)
endif()

OPTION(UMS_BUILD_MAIN "If set, builds main.cpp for manual scheduler testing." OFF)
if (UMS_BUILD_MAIN)
    add_executable(ums main.cpp)
    target_link_libraries(ums PRIVATE ${UMS_LIB})
    target_include_directories(ums PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# add_executable(ums main_2.cpp)

include(CTest)
add_subdirectory(test)
add_subdirectory(benchmark)
