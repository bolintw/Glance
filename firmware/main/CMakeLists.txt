idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES epd_7in5_v2 serial_dump_display gpio esp_driver_spi)
