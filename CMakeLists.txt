cmake_minimum_required(VERSION 3.5)
project(AutoCAN_RT CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include_directories(include)

file(GLOB CORE_SOURCES src/*.cpp)
list(REMOVE_ITEM CORE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

add_library(autocan_core ${CORE_SOURCES})
add_executable(autocan src/main.cpp)
target_link_libraries(autocan autocan_core)

if(NOT WIN32)
    target_link_libraries(autocan pthread)
endif()

option(AUTOCAN_BUILD_TESTS "Build GoogleTest tests" OFF)
if(AUTOCAN_BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    add_executable(autocan_tests tests/test_health.cpp tests/test_csv.cpp tests/test_config.cpp)
    target_link_libraries(autocan_tests autocan_core GTest::GTest GTest::Main)
    if(NOT WIN32)
        target_link_libraries(autocan_tests pthread)
    endif()
    add_test(NAME autocan_tests COMMAND autocan_tests)
endif()
