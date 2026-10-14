idf_component_register(SRCS "main.cpp" "app_state.cpp" "battery.cpp" "buttons.cpp" "peripherals.cpp" "refresh.cpp"
                            "power_status.cpp" "serial_commands.cpp" "setup_flow.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES epd_7in5_v2 serial_dump_display gpio esp_driver_spi wifi_manager time_sync calendar_fetch ics canvas calendar_view refresh_schedule weather_fetch settings
                                  setup_mode setup_view button photos ota_update photo_store fuel_gauge wake_log wake_log_store
                                  run_policy esp_wifi esp_timer esp_driver_gpio esp_adc)
