cmake_minimum_required(VERSION 3.29)
project(USBPortConfiguration)

set(CMAKE_CXX_STANDARD 20)

add_executable(USBPortConfiguration main.cpp)

target_link_libraries(USBPortConfiguration setupapi)
