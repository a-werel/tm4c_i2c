cmake_minimum_required(VERSION 4.1)
project(test_package CXX)

find_package(${PACKAGE_NAME} CONFIG REQUIRED)

add_executable(test_package src/test_package.cpp)
target_link_libraries(test_package ${PACKAGE_NAME}::${PACKAGE_NAME})