add_library(scpi INTERFACE)
target_sources(
    scpi
    INTERFACE scpi_def.c
              scpi_io.c
              bus_format.c
              ext_conn/digital_ctl.c
              ext_conn/i2c_ctl.c
              ext_conn/uart_ctl.c
)
target_link_libraries(scpi INTERFACE scpi_parser)
