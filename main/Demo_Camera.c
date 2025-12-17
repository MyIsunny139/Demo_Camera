#include <stdio.h>
#include "esp_camera.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "spi-sdcard.h"

void app_main(void)
{
    printf("Hello, Camera!\n");
}
