if (UNIX)
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
