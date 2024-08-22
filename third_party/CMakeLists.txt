if (UNIX)
ExternalProject_Add(
    uring
    GIT_REPOSITORY https://github.com/axboe/liburing 
    GIT_TAG liburing-2.7
    PREFIX ${CMAKE_BINARY_DIR}/third_party/uring
    CONFIGURE_COMMAND ./configure --cc=gcc --cxx=g++;
    BUILD_COMMAND  make -j$(nproc);
    INSTALL_COMMAND make install
)
endif()