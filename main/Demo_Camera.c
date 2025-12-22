#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include "esp_camera.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "Video_AVI.h"
#include "App_init.h"
#include "st7789_driver.h"
#include "sdio-sdcard.h"
#include "jpeg_decoder.h"

#define LED_GPIO    GPIO_NUM_2      // LED 引脚

static const char *TAG = "MAIN";
bool led_state = false;     // LED 状态（非static，App_init.c需要extern）

// 短按回调: 切换录制状态
void short_press(int gpio) { 
    if (!video_recorder_is_recording()) {
        // 开始录制
        if (video_recorder_start() == ESP_OK) {
            led_state = true;
            gpio_set_level(LED_GPIO, 1);  // LED 常亮
            ESP_LOGI(TAG, "短按 - 开始录制, LED ON");
        }
    } else {
        // 请求停止录制（实际停止由录制任务完成）
        video_recorder_request_stop();
        ESP_LOGI(TAG, "短按 - 请求停止录制");
        // LED 由录制任务在实际停止时关闭
    }
}

// 长按回调（在 App_init.c 中声明为 extern）
void long_press(int gpio) { 
    ESP_LOGW(TAG, "长按 - 进入配网模式");
    extern void ap_wifi_apcfg(bool enable);
    ap_wifi_apcfg(true);  // 启动 AP 配网
}

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

// 视频录制任务
void video_record_task(void *param)
{
    ESP_LOGI(TAG, "视频录制任务就绪, 短按按键开始/停止录制");
    
    uint32_t loop_count = 0;
    uint32_t last_status_time = 0;
    
    while (1) {
        loop_count++;
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        
        // 录制时每5秒打印一次状态
        if (video_recorder_is_recording() && (now - last_status_time >= 5000)) {
            ESP_LOGI(TAG, "录制状态: %"PRIu32" 帧", video_recorder_get_frame_count());
            last_status_time = now;
        }
        
        // 检查是否收到停止请求
        if (video_recorder_stop_requested()) {
            ESP_LOGI(TAG, "检测到停止请求");
            video_recorder_do_stop();
            // 关闭 LED
            led_state = false;
            gpio_set_level(LED_GPIO, 0);
            ESP_LOGI(TAG, "录制已停止, LED OFF");
        }
        
        // 只有在录制时才获取摄像头帧
        if (video_recorder_is_recording()) {
            camera_fb_t *fb = esp_camera_fb_get();
            if (!fb) {
                ESP_LOGE(TAG, "获取摄像头帧失败");
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            
            // 添加帧到 AVI
            video_recorder_add_frame(fb);
            
            esp_camera_fb_return(fb);
            
            // 控制帧率约 10fps
            
        } else {
            // 未录制时，较长间隔检查
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

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

void app_main(void) 
{
    // 执行所有系统初始化
    ESP_ERROR_CHECK(app_init_all());
    
    ESP_LOGI(TAG, "准备启动视频录制任务...");
    
    // 启动视频录制任务 (短按按键控制录制)
    BaseType_t ret = xTaskCreatePinnedToCore(video_record_task, "video_record", 8192, NULL, 5, NULL, 1);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建视频录制任务失败!");
    } else {
        ESP_LOGI(TAG, "视频录制任务创建成功");
    }
    
    //xTaskCreatePinnedToCore(lcd_show_task, "lcd_show_task", 4096, NULL, 5, NULL, 1);
    //xTaskCreatePinnedToCore(camera_task, "camera_task", 8192, NULL, 5, NULL, 1);
}