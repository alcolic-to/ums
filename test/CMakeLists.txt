include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG       release-1.11.0)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

function(add_cos_test TEST_FILE)
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_FILE})
    target_link_libraries(${TEST_NAME} ${COS_LIBRARIES} gtest_main)
    target_include_directories(${TEST_NAME} PUBLIC ${CMAKE_SOURCE_DIR})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

add_cos_test("${CMAKE_CURRENT_SOURCE_DIR}/basic_test.cpp")
add_cos_test("${CMAKE_CURRENT_SOURCE_DIR}/sleep_test.cpp")
add_cos_test("${CMAKE_CURRENT_SOURCE_DIR}/mutex_test.cpp")
add_cos_test("${CMAKE_CURRENT_SOURCE_DIR}/io_test.cpp")
