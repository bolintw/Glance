idf_component_register(SRCS "button.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES gpio
                    PRIV_REQUIRES esp_timer)
