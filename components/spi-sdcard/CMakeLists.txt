idf_component_register(SRCS "spi-sdcard.c"
                    INCLUDE_DIRS "./include"
                    REQUIRES driver fatfs sdmmc esp_timer
                    )
