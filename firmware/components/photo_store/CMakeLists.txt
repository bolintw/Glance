idf_component_register(SRCS "photo_store.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES display
                    PRIV_REQUIRES photo_format esp_partition)
