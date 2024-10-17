# Usage:
# Create initial cmake build folder: cmake -S . -B .\build
# Create build with desired config: cmake --build .\build --config [Debug/Release/RelWithDebInfo/MinSizeRel] --parallel 8
#
# Clean command: cmake --build .\build --target clean

cmake_minimum_required(VERSION 3.10)
project(user_mode_scheduler)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # -> compile_commands.json

# Enable clang tidy.
find_program(CLANGTIDY clang-tidy)
if(CLANGTIDY)
    set(CLANG_TIDY_OPTIONS ${CLANGTIDY}
        -extra-arg=-Wno-unknown-warning-option
        -extra-arg=-Wno-ignored-optimization-argument
        -extra-arg=-Wno-unused-command-line-argument)

    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_OPTIONS})
    message("clang-tidy configured globally.")
else()
    message(${WARNING_MESSAGE} "clang-tidy requested but executable not found")
endif()

# For quick clang-tidy disable, just uncomment next line.
# set(CMAKE_CXX_CLANG_TIDY "")

# Separate bin directory from build directory.
#
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)

set(CPP_FILES cpu.cpp os_specific.cpp scheduler.cpp task_manager.cpp worker.cpp io.cpp util.cpp cos.cpp spinlock.cpp mutex.cpp condition_variable.cpp)
set(HPP_FILES cpu.h os_specific.h scheduler.h task_manager.h worker.h io_api.h  util.h config.h spinlock.h mutex.h condition_variable.h)
set(ALL_FILES ${CPP_FILES} ${HPP_FILES})

add_library(user_mode_scheduler_lib STATIC ${CPP_FILES})
set(COS_LIBRARIES user_mode_scheduler_lib)

# Create an executable.
add_executable(user_mode_scheduler main.cpp)
target_link_libraries(user_mode_scheduler PRIVATE ${COS_LIBRARIES})
target_include_directories(user_mode_scheduler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if (MSVC)
    add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)
else()
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_definitions(-DDEBUG)
    endif()
endif()

SET(CMAKE_CXX_FLAGS  "-Wall")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-interference-size")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
endif()

enable_testing()
add_subdirectory(test)

message("Disabling clang-tidy for google tests.")
set_target_properties(gmock PROPERTIES CXX_CLANG_TIDY "")
set_target_properties(gmock_main PROPERTIES CXX_CLANG_TIDY "")
set_target_properties(gtest PROPERTIES CXX_CLANG_TIDY "")
set_target_properties(gtest_main PROPERTIES CXX_CLANG_TIDY "")
