#include <stdio.h>
#include "esp_camera.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "ws_server.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "jpeg_decoder.h"

#define BOARD_ESP32S3_GOOUUU 1
#include "camera_pinout.h"

#include "sdio-sdcard.h"
#include "button.h"
#include "driver/gpio.h"
#include "ap_wifi.h"
#include "wifi_manager.h"
#include "esp_websocket_client.h"
#include "st7789_driver.h"
#include "cst816t_driver.h"

#define LED_GPIO    GPIO_NUM_2      // LED 引脚
#define BUTTON_GPIO GPIO_NUM_1     // 按键引脚



static const char *TAG = "MAIN";
static bool led_state = false;     // LED 状态

// 获取按键电平
int get_level(int gpio) { 
    return gpio_get_level(gpio); 
}

// 短按回调: 切换 LED 状态
void short_press(int gpio) { 
    led_state = !led_state;                     // 反转状态
    gpio_set_level(LED_GPIO, led_state);        // 设置 LED
    ESP_LOGI(TAG, "短按 - LED %s", led_state ? "ON" : "OFF");
}

// 长按回调
void long_press(int gpio) { 
    ESP_LOGW(TAG, "长按 - 进入配网模式");
    ap_wifi_apcfg(true);  // 启动 AP 配网

}
void wifi_state_handle(WIFI_STATE state)
{
    if(state == WIFI_STATE_CONNECTED)
    {
        ESP_LOGI(TAG,"Wifi connected");
    }
    else if(state == WIFI_STATE_CONNECTED)
    {
        ESP_LOGI(TAG,"Wifi disconnected");
    }
}

void app_button_init(void)
{
    ESP_LOGI(TAG, "系统启动");
    
    // 初始化 LED GPIO (输出模式)
    gpio_config_t led_io = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_io);
    gpio_set_level(LED_GPIO, 0);  // 初始状态: 关闭
    
    // 初始化按键 GPIO (输入模式)
    gpio_config_t btn_io = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_io);

    // 配置按键
    button_config_t btn = {
        .gpio_num = BUTTON_GPIO,
        .active_level = 0,
        .long_press_time = 3000,
        .getlevel_cb = get_level,
        .short_cb = short_press,
        .long_cb = long_press
    };
    
    // 注册按键
    button_event_set(&btn);
    
    ESP_LOGI(TAG, "按键初始化完成");
    ESP_LOGI(TAG, "短按 GPIO%d -> 切换 LED (GPIO%d)", BUTTON_GPIO, LED_GPIO);
}

camera_fb_t *fb = NULL;

// 简单的全屏刷色测试 -> 改为视频流显示
void lcd_show_task(void *param)
{
    // 申请解码缓冲区 (240x280 RGB565 = 134400 bytes)
    size_t out_buffer_size = 240 * 240 * 2;
    uint8_t *out_buffer = heap_caps_malloc(out_buffer_size, MALLOC_CAP_SPIRAM);
    if (!out_buffer) {
        out_buffer = heap_caps_malloc(out_buffer_size, MALLOC_CAP_DMA);
    }
    memset(out_buffer, 0, out_buffer_size);
    if (!out_buffer) {
        ESP_LOGE(TAG, "LCD缓冲区内存不足");
        vTaskDelete(NULL);
    }

    // 清屏 (填充黑色)
    memset(out_buffer, 0, out_buffer_size);


    esp_jpeg_image_cfg_t jpeg_cfg = {
        .out_format = JPEG_IMAGE_FORMAT_RGB565,
        .out_scale = JPEG_IMAGE_SCALE_0,
        .flags = {
            .swap_color_bytes = 1, // RGB565字节序交换
        },
    };
    esp_jpeg_image_output_t out_img;
    
    while (1) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "获取摄像头帧失败");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        jpeg_cfg.indata = fb->buf;
        jpeg_cfg.indata_size = fb->len;
        jpeg_cfg.outbuf = out_buffer;
        jpeg_cfg.outbuf_size = out_buffer_size;

        esp_err_t ret = esp_jpeg_decode(&jpeg_cfg, &out_img);
        
        if (ret == ESP_OK) {
            // 居中显示 (屏幕高度280，图像高度240，上下各留20)
            // 注意: st7789_flush 的参数是 x_start, x_end, y_start, y_end
            // x_end 和 y_end 是不包含的边界 (例如 0-240 表示 0 到 239)
            st7789_flush(40, 40+out_img.width, 0, 0 + out_img.height, (uint16_t*)out_buffer);
        } else {
            ESP_LOGE(TAG, "JPEG解码失败: %d", ret);
        }

        esp_camera_fb_return(fb);
        // 全速运行，不加延时
    }
}

static uint16_t jpge_id = 0;

void camera_task(void *param)
{
    while (1) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "获取摄像头帧失败");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        ESP_LOGI(TAG, "拍摄成功: %u 字节", fb->len);
        
        // 使用短文件名以兼容 8.3 格式 (FATFS LFN disabled)
        char filename[32];
        sprintf(filename, "img_%03d.jpg", jpge_id);
        ESP_LOGI(TAG, "保存图片到: %s", filename);
        
        if (fb) 
        {
            sd_write_jpeg_file(filename, fb->buf, fb->len);
            esp_camera_fb_return(fb);
            jpge_id++;
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void camera_init(void)
{
    camera_config_t config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,
        
        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,
        
        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_240X240,
        .jpeg_quality = 12,
        .fb_count = 2,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY
    };
    
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "摄像头初始化失败: 0x%x", err);
        return;
    }
    
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_vflip(s, 0);
        s->set_hmirror(s, 0);
    }
    
    ESP_LOGI(TAG, "摄像头初始化成功");
}

void lcd_init(void)
{
    st7789_cfg_t lcd_cfg = {
        .mosi = LCD_MOSI,
        .clk  = LCD_CLK,
        .cs   = LCD_CS,
        .dc   = LCD_DC,
        .rst  = LCD_RST,
        .bl   = LCD_BL,
        .spi_fre = 40000000,
        .width = 240,
        .height = 280,
        .spin = 1,
        .done_cb = NULL,
        .cb_param = NULL
    };
    st7789_driver_hw_init(&lcd_cfg);
    st7789_lcd_backlight(true); // 打开背光
}

void app_main(void) 
{
    ESP_ERROR_CHECK(nvs_flash_init());
    app_button_init();
    ap_wifi_init(wifi_state_handle);
    ESP_ERROR_CHECK(sd_sdio_init());
    lcd_init();
    camera_init();
    xTaskCreatePinnedToCore(lcd_show_task, "lcd_show_task", 4096, NULL, 5, NULL, 1);
    // xTaskCreatePinnedToCore(camera_task, "camera_task", 8192, NULL, 5, NULL, 1);
}