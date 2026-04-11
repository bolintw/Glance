idf_component_register(SRCS "storage.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES spiffs)

spiffs_create_partition_image(storage ${CMAKE_SOURCE_DIR}/spiffs_data FLASH_IN_PROJECT)
