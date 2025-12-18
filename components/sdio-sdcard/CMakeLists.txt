idf_component_register(SRCS "sdio-sdcard.c"
                    INCLUDE_DIRS "./include"
                    REQUIRES driver fatfs sdmmc esp_timer
                    )
