add_library(
    lcd
    lcd.c
)

target_include_directories(lcd
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(lcd PRIVATE -O2)
