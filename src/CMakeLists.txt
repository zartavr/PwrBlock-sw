add_subdirectory(usbpd)
add_subdirectory(scpi)
add_subdirectory(control)
add_subdirectory(threads)
add_subdirectory(indication)
add_subdirectory(bsp)
add_subdirectory(regulator)
add_subdirectory(button)
add_subdirectory(monitor)
add_subdirectory(middleware)
add_subdirectory(storage)
add_subdirectory(sys_status)
add_subdirectory(ntc)
add_subdirectory(usbctrl)

target_sources(power_block PRIVATE app.c)

target_link_libraries(
    power_block
    PUBLIC button
           tinyusb
           scpi
           control
           usbpd
           threads
           indication
           periph_setup
           regulator
           monitor
           middleware
           storage
           sys_status
           ntc
           usbctrl
)
