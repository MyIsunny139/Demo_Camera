idf_component_register(SRCS "App_init.c" "Demo_Camera.c" 
                    INCLUDE_DIRS "."
                    REQUIRES sdio-sdcard button ap_wifi esp_http_server nvs_flash LCD_1.69spi esp_jpeg esp32-camera Video_AVI
                    )
spiffs_create_partition_image(html ../html FLASH_IN_PROJECT)