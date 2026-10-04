idf_component_register(SRCS "weather_fetch.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES weather
                    PRIV_REQUIRES esp_http_client mbedtls)
