add_executable(camera_app
    main.c
)

target_link_libraries(camera_app
    camera
    lcd
    pxp    
)
