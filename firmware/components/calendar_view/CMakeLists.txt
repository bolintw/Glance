idf_component_register(SRCS "calendar_view.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES canvas ics weather
                    PRIV_REQUIRES fonts icons)
