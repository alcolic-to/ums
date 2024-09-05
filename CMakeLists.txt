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
        -extra-arg=-Wno-unused-command-line-argument
        -p)

    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_OPTIONS})
    message("clang-tidy configured globally.")
else()
    message(${WARNING_MESSAGE} "clang-tidy requested but executable not found")
endif()

# Separate bin directory from build directory.
#
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)

set(CPP_FILES cpu.cpp os_specific.cpp scheduler.cpp task_manager.cpp worker.cpp io.cpp sync.cpp util.cpp)
set(HPP_FILES cpu.h os_specific.h scheduler.h task_manager.h worker.h io_api.h sync_api.h util.h config.h)
set(ALL_FILES ${CPP_FILES} ${HPP_FILES})

add_library(user_mode_scheduler_lib STATIC ${CPP_FILES})
set(COS_LIBRARIES user_mode_scheduler_lib)

# Create an executable.
add_executable(user_mode_scheduler main.cpp)
target_link_libraries(user_mode_scheduler PRIVATE ${COS_LIBRARIES})
target_include_directories(user_mode_scheduler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

SET(CMAKE_CXX_FLAGS  "-Wall")
if(MSVC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
endif()

enable_testing()
add_subdirectory(test)
