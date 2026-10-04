idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES epd_7in5_v2 serial_dump_display gpio esp_driver_spi wifi_manager time_sync calendar_fetch ics canvas calendar_view refresh_schedule weather_fetch settings
                                  setup_mode setup_view button photos ota_update)
