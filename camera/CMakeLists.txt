cmake_minimum_required(VERSION 3.16)

project(camera VERSION 1.0.0)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)

add_subdirectory(app)
add_subdirectory(camera)
add_subdirectory(lcd)
add_subdirectory(pxp)
