cmake_minimum_required(VERSION 3.16)

# 交叉编译配置
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)

project(camera_project VERSION 0.4.0)

add_compile_options(-Wall -Wextra)

# 添加子目录
add_subdirectory(common)
add_subdirectory(core)
add_subdirectory(device)
add_subdirectory(service)

add_executable(capture
    main.c
)

target_link_libraries(capture
    PRIVATE
    service
)
