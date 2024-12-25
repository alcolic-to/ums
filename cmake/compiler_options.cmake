if (MSVC)
    add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)
else()
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_definitions(-DDEBUG)
    endif()
endif()

SET(CMAKE_CXX_FLAGS  "-Wall")

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-interference-size")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
endif()