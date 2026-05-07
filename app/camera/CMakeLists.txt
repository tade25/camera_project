add_library(
    camera
    camera.c
)

target_include_directories(camera
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
