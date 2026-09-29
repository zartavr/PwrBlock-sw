add_library(periph_ctl INTERFACE)
target_sources(
    periph_ctl
    INTERFACE digital_ctl.c digital_gpio.c bus_format.c i2c_ctl.c i2c_bus.c
)

target_link_libraries(periph_ctl INTERFACE scpi_parser hal_stm32g0b1)
