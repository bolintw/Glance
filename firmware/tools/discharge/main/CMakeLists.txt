idf_component_register(SRCS "discharge.cpp"
                    PRIV_REQUIRES max17048 esp_driver_gpio esp_driver_i2c esp_wifi esp_netif esp_event esp_timer)
