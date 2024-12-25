OPTION(BUILD_BENCHMARK "If set, builds all benchmarks." ON)
if (NOT BUILD_BENCHMARK)
    return ()
endif()

include(FetchContent)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.9.1)

set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(benchmark)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/benchmark)

message("Disabling clang-tidy for google benchmark.")
set_property(TARGET benchmark PROPERTY CXX_CLANG_TIDY "")
set_property(TARGET benchmark_main PROPERTY CXX_CLANG_TIDY "")

function(add_benchmark BENCHMARK_FILE)
    get_filename_component(BENCHMARK_NAME ${BENCHMARK_FILE} NAME_WE)
    add_executable(${BENCHMARK_NAME} ${BENCHMARK_FILE})
    target_link_libraries(${BENCHMARK_NAME} PRIVATE ${COS_LIBRARIES} benchmark::benchmark)
    target_include_directories(${BENCHMARK_NAME} PUBLIC ${CMAKE_SOURCE_DIR})
endfunction()

add_benchmark("scheduler_benchmark.cpp")
add_benchmark("containers_benchmark.cpp")
add_benchmark("mutex_benchmark.cpp")
add_benchmark("io_benchmark.cpp")
