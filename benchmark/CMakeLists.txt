include(FetchContent)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.9.1)

set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(benchmark)

function(add_benchmark BENCHMARK_FILE)
    get_filename_component(BENCHMARK_NAME ${BENCHMARK_FILE} NAME_WE)
    add_executable(${BENCHMARK_NAME} ${BENCHMARK_FILE})
    target_link_libraries(${BENCHMARK_NAME} PRIVATE ${COS_LIBRARIES} benchmark::benchmark)
    target_include_directories(${BENCHMARK_NAME} PUBLIC ${CMAKE_SOURCE_DIR})
endfunction()

# Benchmarks
#
add_benchmark("${CMAKE_CURRENT_SOURCE_DIR}/basic_benchmark.cpp")
add_benchmark("${CMAKE_CURRENT_SOURCE_DIR}/containers_benchmark.cpp")
add_benchmark("${CMAKE_CURRENT_SOURCE_DIR}/mutex_benchmark.cpp")
add_benchmark("${CMAKE_CURRENT_SOURCE_DIR}/io_benchmark.cpp")
