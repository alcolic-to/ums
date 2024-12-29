# Usage:
# Create initial cmake build folder: cmake -S . -B .\build
# Create build with desired config: cmake --build .\build --config [Debug/Release/RelWithDebInfo/MinSizeRel]
#
# Clean command: cmake --build .\build --target clean

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

include(cmake/sanitizers.cmake)
include(cmake/compiler_options.cmake)

set(CPP_FILES os_specific.cpp scheduler.cpp task_manager.cpp worker.cpp io.cpp util.cpp spinlock.cpp mutex.cpp shared_mutex.cpp condition_variable.cpp ums.cpp)
set(HPP_FILES os_specific.h scheduler.h task_manager.h worker.h io_api.h  util.h config.h spinlock.h mutex.h shared_mutex.h condition_variable.h ums.h options.h)
set(ALL_FILES ${CPP_FILES} ${HPP_FILES})

add_library(ums_lib STATIC ${CPP_FILES})

if (UNIX)
    add_subdirectory(third_party)
    target_link_libraries(ums_lib PRIVATE uring)
    add_dependencies(ums_lib uring_project)
endif()

set(COS_LIBRARIES ums_lib)

OPTION(BUILD_MAIN "If set, builds main.cpp for manual scheduler testing." ON)
if (BUILD_MAIN)
    add_executable(ums main.cpp)
    target_link_libraries(ums PRIVATE ${COS_LIBRARIES})
    target_include_directories(ums PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()

include(CTest)
add_subdirectory(test)
add_subdirectory(benchmark)
