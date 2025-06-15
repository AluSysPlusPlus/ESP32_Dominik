idf_component_register(
    SRCS "hello_HTTP_SIM.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        esp_driver_uart
        spi_flash
)
