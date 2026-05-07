add_library(
    lcd
    lcd.c
)

target_include_directories(lcd
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
