idf_component_register(SRCS "settings.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES photo_rotation
                    PRIV_REQUIRES nvs_flash)
