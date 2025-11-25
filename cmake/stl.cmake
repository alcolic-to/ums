set(UMS_STL_PATH "" CACHE STRING "Path to STL common lib.")
if (NOT UMS_STL_PATH STREQUAL "")
    message("Using external STL lib ${UMS_STL_PATH} for UMS.")
    target_include_directories(${UMS_LIB} SYSTEM PUBLIC ${UMS_STL_PATH})
    return()
endif()

include(FetchContent)

FetchContent_Declare(
    stl
    GIT_REPOSITORY https://github.com/alcolic-to/stl.git
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE)

FetchContent_MakeAvailable(stl)

message("Using internal STL lib ${stl_SOURCE_DIR} for UMS.")
target_include_directories(${UMS_LIB} SYSTEM PUBLIC ${stl_SOURCE_DIR})
