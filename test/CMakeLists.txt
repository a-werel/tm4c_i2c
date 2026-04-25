cmake_minimum_required(VERSION 4.1)
project(test CXX)

enable_testing()
include(GoogleTest)

find_package(GTest REQUIRED CONFIG)

file(GLOB SRC_FILES "src/*.cpp")
add_executable(test ${SRC_FILES})
target_link_libraries(test GTest::gtest GTest::gtest_main ${PACKAGE_NAME})

gtest_discover_tests(test)