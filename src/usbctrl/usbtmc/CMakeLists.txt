add_library(usbtmc INTERFACE)

target_sources(
    usbtmc INTERFACE usb_descriptors.c usbtmc_app.c usbtmc_exchange.c
)
