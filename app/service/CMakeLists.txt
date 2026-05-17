add_library(service
    STATIC
    capture/capture_service.c
    display/display_service.c
)

target_include_directories(service
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/capture
    ${CMAKE_CURRENT_SOURCE_DIR}/display
)

target_link_libraries(service
    PUBLIC
    device
    core
)
