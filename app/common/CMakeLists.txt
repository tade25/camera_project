add_library(common
    STATIC
    ring_buffer/ring_buffer.c
)

target_include_directories(common
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/ring_buffer
)

target_link_libraries(common
    PUBLIC
    pthread
)
