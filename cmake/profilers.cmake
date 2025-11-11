# To enable tracy profiler, build project with UMS_ENABLE_TRACY=ON
# You will also need tracy server, GUI etc. installed, since we are not building it from scratch (I tried couple of times and it failed, so I gave up).
# For windows, found needed binaries under releases https://github.com/wolfpld/tracy. Note that server (downloaded binaries) and client version (one compiled by UMS) needs to be the same.
# Also, on windows, make sure to run program as administrator so tracy can collect stack traces.

OPTION(UMS_ENABLE_TRACY "If set, enables tracy profiler and adds function to enable tracy for projects." OFF)
if (NOT UMS_ENABLE_TRACY)
    return()
endif()

include(FetchContent)

FetchContent_Declare(
    tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG        v0.12.2
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE)

FetchContent_MakeAvailable(tracy)

if(MSVC)
    target_compile_options(TracyClient PRIVATE /W0)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(TracyClient PRIVATE -w)
endif()

set_property(TARGET TracyClient PROPERTY CXX_CLANG_TIDY "") # Disable clang tidy for tracy.

function(add_tracy PROJECT)
    target_include_directories(${PROJECT} SYSTEM PRIVATE ${tracy_SOURCE_DIR}/public)
    target_sources(${PROJECT} PRIVATE ${tracy_SOURCE_DIR}/public/TracyClient.cpp)
    target_link_libraries(${PROJECT} PUBLIC TracyClient)
    target_compile_definitions(${PROJECT} PRIVATE TRACY_ENABLE TRACY_CALLSTACK)

    if(MSVC)
        target_compile_options(${PROJECT} PRIVATE /W0)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${PROJECT} PRIVATE -w)
    endif()

    message("Added tracy profiler for ${PROJECT}. Run profiled program as administrator on windows in order to get stack traces.")
endfunction()
