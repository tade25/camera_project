add_library(device
    STATIC
    camera/camera.c
    lcd/lcd.c
    pxp/pxp.c
)

target_include_directories(device
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/camera
    ${CMAKE_CURRENT_SOURCE_DIR}/lcd
    ${CMAKE_CURRENT_SOURCE_DIR}/pxp
)
