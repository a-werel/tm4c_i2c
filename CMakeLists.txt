cmake_minimum_required(VERSION 4.1)
project(${PACKAGE_NAME} CXX)

find_package(error CONFIG REQUIRED)
find_package(i_callback CONFIG REQUIRED)
find_package(i_i2c CONFIG REQUIRED)
find_package(i_irq_handler CONFIG REQUIRED)
find_package(i_processable CONFIG REQUIRED)
find_package(i_serial_port CONFIG REQUIRED)
find_package(i_sysclk CONFIG REQUIRED)
find_package(locator CONFIG REQUIRED)
find_package(tm4c_system CONFIG REQUIRED)


file(GLOB SRC_FILES "src/*.cpp")
add_library(${PACKAGE_NAME} ${SRC_FILES})
target_include_directories(${PACKAGE_NAME} PUBLIC include)
target_link_libraries(
    ${PACKAGE_NAME}
    error::error
    i_callback::i_callback
    i_i2c::i_i2c
    i_irq_handler::i_irq_handler
    i_processable::i_processable
    i_serial_port::i_serial_port
    i_sysclk::i_sysclk
    locator::locator
    tm4c_system::tm4c_system
)

file(GLOB HEADER_FILES "include/ne/*.h")
foreach(HEADER_FILE ${HEADER_FILES})
    file(RELATIVE_PATH HEADER_FILE_RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${HEADER_FILE})
    list(APPEND HEADER_FILES_RELATIVE ${HEADER_FILE_RELATIVE})
endforeach()
set_target_properties(${PACKAGE_NAME} PROPERTIES PUBLIC_HEADER "${HEADER_FILES_RELATIVE}")

install(TARGETS ${PACKAGE_NAME} PUBLIC_HEADER DESTINATION "include/ne")

if(NOT BUILD_TESTING STREQUAL OFF)
    add_subdirectory(test)
endif()
