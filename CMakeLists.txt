# Usage:
# Create initial cmake build folder: cmake [-G Ninja] -B build -DCMAKE_BUILD_TYPE=[Debug/Release] -DCMAKE_CXX_COMPILER=clang++-20 -DCMAKE_C_COMPILER=clang-20
# Create build with desired config: cmake --build build
# On linux, if you want to use fast I/O, you have to use makefiles for build, since lib_io_uring is build that way, and I am not patient enough to make it build with ninja. Also, you need to set UMS_BUILD_URING
#
# Clean command: cmake --build build --target clean

# If you want to play around with scheduler, you can just enable UMS_BUILD_MAIN which builds main.cpp file.

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

set(CPP_FILES os_specific.cpp task.cpp scheduler.cpp worker.cpp io.cpp spinlock.cpp mutex.cpp shared_mutex.cpp condition_variable.cpp ums.cpp)
set(HPP_FILES os_specific.h task.h scheduler.h async.h worker.h io_api.h  util.h config.h spinlock.h mutex.h shared_mutex.h condition_variable.h ums.h options.h)
set(ALL_FILES ${CPP_FILES} ${HPP_FILES})

add_library(ums STATIC ${CPP_FILES})
set(UMS_LIB ums)

include(cmake/stl.cmake)

add_subdirectory(third_party)
if (UMS_BUILD_URING)
    target_link_libraries(ums PRIVATE uring)
    add_dependencies(ums uring_project)
    add_compile_definitions(UMS_IO_URING_ENABLED=1) # Add global define for uring, to be able to disable it in a source code.
endif()

include(cmake/profilers.cmake)
if (UMS_ENABLE_TRACY)
    add_tracy(ums)
endif()

OPTION(UMS_BUILD_MAIN "If set, builds main.cpp for manual scheduler testing." OFF)
if (UMS_BUILD_MAIN)
    add_executable(ums_main main.cpp)
    target_link_libraries(ums_main PRIVATE ${UMS_LIB})
    target_include_directories(ums_main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()

include(CTest)
add_subdirectory(test)
add_subdirectory(benchmark)
