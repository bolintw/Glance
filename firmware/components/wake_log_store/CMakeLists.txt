idf_component_register(SRCS "wake_log_store.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES wake_log
                    PRIV_REQUIRES esp_partition photo_format photo_store)
