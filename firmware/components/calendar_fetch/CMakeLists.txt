idf_component_register(SRCS "calendar_fetch.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES ics esp_http_client
                    PRIV_REQUIRES mbedtls esp_timer)
