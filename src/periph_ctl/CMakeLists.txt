add_library(periph_ctl INTERFACE)
target_sources(periph_ctl INTERFACE digital_ctl.c digital_gpio.c)

target_link_libraries(periph_ctl INTERFACE scpi_parser hal_stm32g0b1)
