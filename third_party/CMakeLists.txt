OPTION(BUILD_URING "If set, builds uring lib. Not working currently. Call Topalovic." OFF)
if (NOT BUILD_URING)
    return()
endif()

if (LINUX)
# Add global define for uring, to be able to disable it in a source code.
add_compile_definitions(IO_URING_ENABLED=1)
include(ExternalProject)

set(URING_PROJECT_PREFIX "${CMAKE_BINARY_DIR}/third_party/uring_project")
set(URING_PROJECT_SOURCE "${URING_PROJECT_PREFIX}/src/uring_project")
ExternalProject_Add(
    uring_project
    GIT_REPOSITORY https://github.com/axboe/liburing 
    GIT_TAG liburing-2.7
    PREFIX ${URING_PROJECT_PREFIX}
    CONFIGURE_COMMAND bash -c "cd ${URING_PROJECT_SOURCE} && ./configure --cc=gcc --cxx=g++;"
    BUILD_COMMAND  bash -c "cd ${URING_PROJECT_SOURCE} && make -j$(nproc);"
    INSTALL_COMMAND bash -c "cd ${URING_PROJECT_SOURCE} && sudo make install;"
)
endif()
