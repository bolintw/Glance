idf_component_register(SRCS "setup_mode.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES settings
                    PRIV_REQUIRES setup_page captive_dns esp_wifi esp_netif esp_event esp_http_server lwip
                                  esp_timer photo_store photo_format photos station_watch lan_access)
