idf_component_register(SRCS "main.c"
                    REQUIRES nvs_flash esp_wifi esp_event esp_system driver 
                    INCLUDE_DIRS ".")
