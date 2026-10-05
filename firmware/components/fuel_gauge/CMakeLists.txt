idf_component_register(SRCS "fuel_gauge.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES max17048 esp_driver_i2c)
