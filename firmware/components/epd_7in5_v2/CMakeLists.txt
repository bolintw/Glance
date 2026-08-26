idf_component_register(SRCS "epd_7in5_v2.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES gpio spi display esp_timer)
