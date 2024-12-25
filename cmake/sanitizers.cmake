OPTION(ENABLE_CLANG_TIDY "Enable static analysis with clang-tidy." OFF)
if (NOT ENABLE_CLANG_TIDY)
    return()
endif()

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
