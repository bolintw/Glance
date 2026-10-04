idf_component_register(SRCS "time_sync.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES esp_netif
                    PRIV_REQUIRES esp_timer)
