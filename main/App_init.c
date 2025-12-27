#include "App_init.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "button.h"
#include "driver/gpio.h"
#include "ap_wifi.h"
#include "wifi_manager.h"
#include "sdio-sdcard.h"
#include "st7789_driver.h"
#include "cst816t_driver.h"
#include "Video_AVI.h"
#include "app_wss_client.h"
#include "MAX98367A.h"
#include "INMP441.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#define BOARD_ESP32S3_GOOUUU 1
#include "camera_pinout.h"

QueueHandle_t audio_data_queue = NULL;      // 麦克风 → WebSocket
QueueHandle_t audio_playback_queue = NULL;  // WebSocket → 扬声器



#define LED_GPIO    GPIO_NUM_2      // LED 引脚
#define BUTTON_GPIO GPIO_NUM_1     // 按键引脚

// 视频录制配置
#define VIDEO_FPS           10
#define VIDEO_WIDTH         240
#define VIDEO_HEIGHT        240

static const char *TAG = "APP_INIT";
extern bool led_state;  // 在 Demo_Camera.c 中定义

// 获取按键电平
int get_level(int gpio) { 
    return gpio_get_level(gpio); 
}

// 短按回调: 切换录制状态
void short_press(int gpio);

// 长按回调
void long_press(int gpio);

// WiFi状态回调
void wifi_state_handle(WIFI_STATE state)
{
    if(state == WIFI_STATE_CONNECTED)
    {
        ESP_LOGI(TAG, "WiFi connected, 尝试连接 WebSocket...");
        // WiFi 连接成功后，尝试连接 WebSocket
        if (!wss_client_is_connected()) {
            wss_client_connect();
        }
    }
    else if(state == WIFI_STATE_DISCONNECTED)
    {
        ESP_LOGW(TAG, "WiFi disconnected");
    }
}

void app_button_init(void)
{
    // 配置 LED 引脚
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(LED_GPIO, 0);  // 初始关闭 LED

    // 配置按键 GPIO 为输入模式
    gpio_config_t btn_io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,    // 内部上拉
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_io_conf);

    // 配置并注册按键
    button_config_t btn_cfg = {
        .gpio_num = BUTTON_GPIO,
        .active_level = 0,              // 按下为低电平
        .long_press_time = 3000,        // 3秒长按
        .getlevel_cb = get_level,
        .short_cb = short_press,
        .long_cb = long_press
    };
    button_event_set(&btn_cfg);
    ESP_LOGI(TAG, "按键初始化成功");
}


void camera_init(void)
{
    camera_config_t camera_config = {
        .pin_pwdn     = CAM_PIN_PWDN,
        .pin_reset    = CAM_PIN_RESET,
        .pin_xclk     = CAM_PIN_XCLK,
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
        .pin_href  = CAM_PIN_HREF,
        .pin_pclk  = CAM_PIN_PCLK,

        .xclk_freq_hz = 20000000,
        .ledc_timer   = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_JPEG,
        .frame_size   = FRAMESIZE_240X240,  // 240x240
        .jpeg_quality = 15,                 // 0-63, 越小质量越好
        .fb_count     = 3,                  // 帧缓冲数量
        .grab_mode    = CAMERA_GRAB_WHEN_EMPTY, // 缓冲区空时才获取新帧，避免溢出
        .fb_location  = CAMERA_FB_IN_PSRAM
    };

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "摄像头初始化失败: 0x%x", err);
        return;
    }

    // 获取传感器设置
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_vflip(s, 0);
        s->set_hmirror(s, 0);
    }
    
    // 清空摄像头缓冲区，避免 FB-OVF
    for (int i = 0; i < 3; i++) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            esp_camera_fb_return(fb);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    ESP_LOGI(TAG, "摄像头初始化成功");
}



void lcd_init(void)
{

     ESP_LOGI(TAG, "初始化 LCD...");

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
    ESP_LOGI(TAG, "LCD初始化成功");

}



esp_err_t app_video_recorder_init(void)
{
    video_recorder_config_t recorder_cfg = {
        .width = VIDEO_WIDTH,
        .height = VIDEO_HEIGHT,
        .fps = VIDEO_FPS,
        .max_frames = 3000,     // 最大5分钟 @ 10fps
        .save_path = "/0:/"
    };
    esp_err_t ret = video_recorder_init(&recorder_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "视频录制器初始化成功");
    } else {
        ESP_LOGE(TAG, "视频录制器初始化失败: 0x%x", ret);
    }
    return ret;
}

// WebSocket 服务器地址（根据实际情况修改）
#define WSS_SERVER_URI  "ws://192.168.0.222:8080/test/esp32"

esp_err_t app_wss_client_init(void)
{
    esp_err_t ret = wss_client_init(WSS_SERVER_URI);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WebSocket 客户端初始化失败");
        return ret;
    }
    
    // 仅初始化，不立即连接
    // WebSocket 连接将在 WiFi 连接成功后由回调触发
    ESP_LOGI(TAG, "WebSocket 客户端初始化完成，等待 WiFi 连接...");
    
    return ESP_OK;
}

esp_err_t app_init_all(void)
{
    esp_err_t ret;

    // 1. 初始化 NVS（必须最先，WiFi 需要）
    ret = nvs_flash_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS初始化失败: 0x%x", ret);
        return ret;
    }
    ESP_LOGI(TAG, "NVS初始化成功");

    // 2. 初始化按键
    app_button_init();

    // 3. 初始化 SD 卡（不依赖网络）
    ret = sd_sdio_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD卡初始化失败: 0x%x", ret);
        return ret;
    }
    ESP_LOGI(TAG, "SD卡初始化成功");

    // // 4. 初始化 LCD（不依赖网络）
    // lcd_init();

    // 5. 初始化摄像头（不依赖网络）
    camera_init();



    // 6. 初始化视频录制器（不依赖网络）
    ret = app_video_recorder_init();
    if (ret != ESP_OK) 
    {
        return ret;
    }
    
    //? 2. 初始化 I2S 接收（麦克风）
    i2s_rx_init();
    
    //? 3. 最后初始化 I2S 发送（喇叭）- 防止初始化喙音
    i2s_tx_init();

    audio_data_queue = xQueueCreate(5, sizeof(uint8_t) * BUF_SIZE);
    if (audio_data_queue == NULL) {
        ESP_LOGE("MAIN", "Failed to create audio_data_queue");
    }
    
    // 创建音频播放队列（WebSocket → 扬声器）
    audio_playback_queue = xQueueCreate(5, sizeof(uint8_t) * BUF_SIZE);
    if (audio_playback_queue == NULL) {
        ESP_LOGE("MAIN", "Failed to create audio_playback_queue");
    }

    // 7. 初始化 WebSocket 客户端（仅初始化，不连接）
    app_wss_client_init();

    // 8. 初始化 WiFi（放在最后，包含阻塞等待连接）
    // WiFi 连接成功后会通过回调触发 WebSocket 连接
    ap_wifi_init(wifi_state_handle);
    ESP_LOGI(TAG, "WiFi初始化完成");

    ESP_LOGI(TAG, "所有系统初始化完成");
    return ESP_OK;
}
