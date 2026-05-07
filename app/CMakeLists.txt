cmake_minimum_required(VERSION 3.16)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)

project(camera_project VERSION 0.2.0)

add_subdirectory(camera)
add_subdirectory(lcd)
add_subdirectory(pxp)

add_executable(capture
    main.c
)

target_link_libraries(capture
    camera
    lcd
    pxp    
)
