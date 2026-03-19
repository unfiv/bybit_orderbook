cmake_minimum_required(VERSION 3.14)
project(bybit_orderbook)

message(STATUS "STANDARD FROM CONAN: ${CMAKE_CXX_STANDARD}")

if(CMAKE_CXX_STANDARD AND NOT "${CMAKE_CXX_STANDARD}" STREQUAL "23")
    message(FATAL_ERROR "STOP! Conan wants ${CMAKE_CXX_STANDARD}, but we need 23!")
endif()

set(CMAKE_CXX_STANDARD 23)

find_package(Boost 1.86.0 REQUIRED)
find_package(nlohmann_json 3.12.0 REQUIRED)
find_package(OpenSSL 3.2.1 REQUIRED)

include_directories("src")
add_subdirectory(src)

if(MSVC)
    target_compile_options(bybit_orderbook PRIVATE /bigobj)
endif()

add_definitions(-D_WIN32_WINNT=0x0A00) # Target Windows 10/11
