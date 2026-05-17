add_library(core
    STATIC
    frame_pipeline/frame_pipeline.c
)

target_include_directories(core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/frame_pipeline
)

target_link_libraries(core
    PUBLIC
    common
)
