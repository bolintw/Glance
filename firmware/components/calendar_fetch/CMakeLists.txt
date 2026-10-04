idf_component_register(SRCS "calendar_fetch.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES ics
                    PRIV_REQUIRES esp_http_client mbedtls)
