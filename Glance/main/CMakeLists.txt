idf_component_register(SRCS "main.cpp" "photo.c"
                    INCLUDE_DIRS "."
                    REQUIRES epd_7in5_v2 spi wifi time_manager config config_reader spiffs)

spiffs_create_partition_image(storage ../spiffs_data FLASH_IN_PROJECT)
