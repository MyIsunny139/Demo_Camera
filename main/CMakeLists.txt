idf_component_register(SRCS "Demo_Camera.c" 
                    INCLUDE_DIRS "."
                    REQUIRES spi-sdcard button ap_wifi esp_http_server nvs_flash
                    )
spiffs_create_partition_image(html ../html FLASH_IN_PROJECT)