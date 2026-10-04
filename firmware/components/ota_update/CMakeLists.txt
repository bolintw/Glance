idf_component_register(SRCS "ota_update.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES ota_manifest app_update esp_app_format esp_https_ota esp_http_client mbedtls)
