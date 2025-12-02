OPTION(UMS_BUILD_BENCHMARK "If set, builds all benchmarks." OFF)
if (NOT UMS_BUILD_BENCHMARK)
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

function(add_ums_benchmark BENCHMARK_FILE)
    get_filename_component(BENCHMARK_NAME ${BENCHMARK_FILE} NAME_WE)
    add_executable(${BENCHMARK_NAME} ${BENCHMARK_FILE})
    target_link_libraries(${BENCHMARK_NAME} PRIVATE ${UMS_LIB} benchmark::benchmark)
    target_include_directories(${BENCHMARK_NAME} PUBLIC ${CMAKE_SOURCE_DIR})
endfunction()

# add_ums_benchmark("bm_scheduler.cpp")
# add_ums_benchmark("bm_containers.cpp")
# add_ums_benchmark("bm_mutex.cpp")
# add_ums_benchmark("bm_io.cpp")
add_ums_benchmark("bm_task_scheduling.cpp")
add_ums_benchmark("bm_task_exec_1.cpp")
add_ums_benchmark("bm_task_exec_2.cpp")
add_ums_benchmark("bm_task_exec_3.cpp")
add_ums_benchmark("bm_task_exec_4.cpp")
add_ums_benchmark("bm_task_exec_5.cpp")
add_ums_benchmark("bm_task_exec_6.cpp")
add_ums_benchmark("bm_task_exec_7.cpp")
add_ums_benchmark("bm_e2e.cpp")
