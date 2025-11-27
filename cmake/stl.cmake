# Sets include directory for external STL lib.
# User can provide custom STL lib path if it is already installed by setting UMS_STL_PATH variable.
set(UMS_STL_PATH "" CACHE STRING "Path to STL common lib.")
if (UMS_STL_PATH STREQUAL "")
    include(FetchContent)

    FetchContent_Declare(
        stl
        GIT_REPOSITORY https://github.com/alcolic-to/stl.git
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE)

    FetchContent_MakeAvailable(stl)

    message("Using internal STL lib ${stl_SOURCE_DIR} for UMS.")
    set(UMS_STL_PATH ${stl_SOURCE_DIR} CACHE STRING "Path to STL common lib." FORCE)
    message("UMS_STL_PATH value: ${UMS_STL_PATH}")
else ()
    set(UMS_STL_PATH ${UMS_STL_PATH} CACHE STRING "Path to STL common lib." FORCE)
    message("Using external STL lib ${UMS_STL_PATH} for UMS.")
endif ()

target_include_directories(${UMS_LIB} SYSTEM PUBLIC ${UMS_STL_PATH})