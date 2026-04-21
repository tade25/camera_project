add_library(
    pxp
    pxp.c
)

target_include_directories(pxp
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
