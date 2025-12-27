idf_component_register(SRCS "app_wss_client.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_websocket_client esp32-camera esp_event MAX98367A freertos)
