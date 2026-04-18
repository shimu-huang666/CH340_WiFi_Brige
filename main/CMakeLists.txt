idf_component_register(SRCS "main.c"
                          "../components/wifi_AP.c"
                    REQUIRES nvs_flash esp_wifi esp_event esp_system driver lwip
                    INCLUDE_DIRS "." "../components")
