add_subdirectory(third_party)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)
include(Utils)

add_cos_test("${CMAKE_CURRENT_SOURCE_DIR}/basic_test.cpp")
