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
#include "app_wss_client.h"
#include "MAX98367A.h"
#include "INMP441.h"


#define LED_GPIO    GPIO_NUM_2      // LED 引脚

extern   QueueHandle_t audio_data_queue;      // 麦克风 → WebSocket
extern   QueueHandle_t audio_playback_queue;  // WebSocket → 扬声器

static const char *TAG = "MAIN";
bool led_state = false;     // LED 状态（非static，App_init.c需要extern）
static volatile bool s_capture_once = false;  // 单次拍照触发标志
static volatile bool s_capture_busy = false;  // 拍照处理中标志
static volatile bool s_capture_upload = false; // 拍照后上传标志

#define camere_task_mode 1  // 0 = 连续上传 1= 单次上传

// 短按回调: 拍一张照片
void short_press(int gpio) 
{ 
    // 如果正在处理上一张照片，忽略本次按键
    if (s_capture_busy) {
        ESP_LOGW(TAG, "短按 - 正在处理，忽略");
        return;
    }
    s_capture_once = true;  // 设置单次拍照标志
    s_capture_upload = !s_capture_upload; // 设置上传标志
    
    gpio_set_level(LED_GPIO, 1);  // LED 闪烁提示
    ESP_LOGI(TAG, "短按 - 拍照");
}

// 长按回调（在 App_init.c 中声明为 extern）
void long_press(int gpio) 
{ 
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
    if (!out_buffer) 
    {
        out_buffer = heap_caps_malloc(out_buffer_size, MALLOC_CAP_DMA);
    }
    memset(out_buffer, 0, out_buffer_size);
    if (!out_buffer) 
    {
        ESP_LOGE(TAG, "LCD缓冲区内存不足");
        vTaskDelete(NULL);
    }

    // 清屏 (填充黑色)
    memset(out_buffer, 0, out_buffer_size);


    esp_jpeg_image_cfg_t jpeg_cfg = 
    {
        .out_format = JPEG_IMAGE_FORMAT_RGB565,
        .out_scale = JPEG_IMAGE_SCALE_0,
        .flags = {
            .swap_color_bytes = 1, // RGB565字节序交换
        },
    };
    esp_jpeg_image_output_t out_img;
    
    while (1) 
    {
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
        
        if (ret == ESP_OK) 
        {
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
    ESP_LOGI(TAG, "拍照任务就绪，短按按键拍一张照片");
    
    // 启动时先测试摄像头是否正常工作
    ESP_LOGI(TAG, "测试摄像头...");
    camera_fb_t *test_fb = esp_camera_fb_get();
    if (test_fb) {
        ESP_LOGI(TAG, "摄像头测试成功: %ux%u, %u bytes", test_fb->width, test_fb->height, test_fb->len);
        esp_camera_fb_return(test_fb);
    } else {
        ESP_LOGE(TAG, "摄像头测试失败!");
    }
    
    while (1) 
    {
        // 等待拍照触发
        #ifdef camere_task_mode
        if(camere_task_mode==0)
        {
            if(s_capture_upload == false) 
            {
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }
        }
        if(camere_task_mode==1)
        {
            if (s_capture_once == false) 
            {
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }
        }
        #endif
        // 设置处理中标志，清除触发标志
        s_capture_busy = true;
        s_capture_once = false;
        
        ESP_LOGI(TAG, "开始获取摄像头帧...");
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "获取摄像头帧失败");
            s_capture_busy = false;  // 清除处理中标志，允许重试
            gpio_set_level(LED_GPIO, 0);  // 关闭LED
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        ESP_LOGI(TAG, "拍摄成功: %u 字节", fb->len);
        
        // 1. 保存到 SD 卡
        char filename[32];
        sprintf(filename, "img_%03d.jpg", jpge_id);
        sd_write_jpeg_file(filename, fb->buf, fb->len);
        ESP_LOGI(TAG, "保存图片到: %s", filename);
        
        // 2. 通过 WebSocket 上传
        if (wss_client_is_connected()) {
            if (wss_client_upload_image(fb) == ESP_OK) {
                ESP_LOGI(TAG, "图片已上传到服务器");
            }
        }
        
        esp_camera_fb_return(fb);
        jpge_id++;
        
        gpio_set_level(LED_GPIO, 0);  // 拍照完成，关闭LED
        s_capture_busy = false;       // 清除处理中标志

        if(camere_task_mode==0)
        {
            vTaskDelay(pdMS_TO_TICKS(500));  // 连续模式下稍作延时
        }

        ESP_LOGI(TAG, "拍照完成，等待下次按键");
    }
}



uint8_t buf[BUF_SIZE];
void i2s_read_task(void *pvParameters)
{
    size_t bytes_read = 0;
    size_t bytes_written = 0;
 
    //? 一次性读取buf_size数量的音频，即dma最大搬运一次的数据量
    //? 读取后检测音频能量，如果有有效声音才应用增益并发送
    while (1) 
    {
        if (i2s_channel_read(rx_handle, buf, BUF_SIZE, &bytes_read, 100) == ESP_OK)
        {
            //? 1. 检测音频能量（判断是否有有效声音）
            int32_t *samples = (int32_t *)buf;
            size_t sample_count = bytes_read / sizeof(int32_t);
            int64_t total_energy = 0;
            
            //? 计算音频能量（所有采样点的绝对值之和）
            for (size_t i = 0; i < sample_count; i++) {
                total_energy += abs(samples[i]);
            }
            
            //? 计算平均能量
            int64_t avg_energy = total_energy / sample_count;
            
            //? 只有当平均能量超过阈值时才处理和发送数据
            //? 阈值设置：50000 表示静音或极小声音时不发送
            const int64_t AUDIO_ENERGY_THRESHOLD = 50000;
            
            if (avg_energy > AUDIO_ENERGY_THRESHOLD) {
                //? 2. 应用音量增益（只对有效音频增益）
                // max98367a_apply_gain(buf, bytes_read);
                // for(int i=0;i<bytes_read;i++)
                // {
                //     printf("%d ",buf[i]);
                // }
                // printf("\r\n");

                //? 3. 发送完整音频数据到队列（2048字节完整DMA帧）
                if (audio_data_queue != NULL) {
                    //? 非阻塞方式发送完整缓冲区

                    if (xQueueSend(audio_data_queue, buf, 0) != pdPASS) {
                        //? 队列满，丢弃数据（优先保证实时性）
                        // ESP_LOGW("AUDIO", "Queue full, dropping audio frame");
                    }
                }
            }
            // else {
            //     //? 音频能量过低，不发送数据
            //     // ESP_LOGD("AUDIO", "Audio energy too low: %lld, skipping", avg_energy);
            // }
        }
        //? 移除延迟，让任务以最快速度运行，提高音频实时性
         vTaskDelay(pdMS_TO_TICKS(5)); 
    }
    vTaskDelete(NULL);
}

void i2s_send_task(void *pvParameters)
{
    uint8_t playback_buf[BUF_SIZE] = {0};  // 播放缓冲区（2048字节完整DMA帧）
    size_t bytes_written = 0;

    while (1) 
    {
        // 从播放队列获取音频数据
        if (audio_playback_queue != NULL) {
            // 等待从队列接收完整音频帧，超时100ms
            if (xQueueReceive(audio_playback_queue, playback_buf, pdMS_TO_TICKS(100)) == pdTRUE) {
                // 播放接收到的完整音频数据（2048字节）
                max98367a_apply_gain(playback_buf, BUF_SIZE);
                esp_err_t ret = i2s_channel_write(tx_handle, playback_buf, BUF_SIZE, &bytes_written, portMAX_DELAY);
                if (ret != ESP_OK) {
                    ESP_LOGE("I2S_SEND", "Failed to write audio data: %s", esp_err_to_name(ret));
                } else if (bytes_written != BUF_SIZE) {
                    ESP_LOGW("I2S_SEND", "Incomplete write: %d/%d bytes", bytes_written, BUF_SIZE);
                }
            }
        }
        else {
            // 队列未创建，等待
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    vTaskDelete(NULL);
}

//? 音频直通任务：麦克风直接输出到扬声器（硬件测试）
//? 不经过队列，最低延迟，用于测试麦克风和扬声器是否正常工作
void audio_passthrough_task(void *pvParameters)
{
    uint8_t passthrough_buf[BUF_SIZE] = {0};
    size_t bytes_read = 0;
    size_t bytes_written = 0;
    
    ESP_LOGI("AUDIO_PASSTHROUGH", "Direct passthrough task started - Mic -> Speaker");
    ESP_LOGI("AUDIO_PASSTHROUGH", "Sample rate: %d Hz, Bit width: %d, Buffer: %d bytes", 
             MAX98367A_SAMPLE_RATE, MAX98367A_BIT_WIDTH, BUF_SIZE);
    
    while (1) 
    {
        //? 直接从麦克风读取数据
        if (i2s_channel_read(rx_handle, passthrough_buf, BUF_SIZE, &bytes_read, 100) == ESP_OK)
        {
            //? 应用噪声过滤（去除低能量杂音）
            inmp441_filter_noise(passthrough_buf, bytes_read);
            
            // max98367a_set_gain(3.0f);  //? 设置适度增益，避免过大音量损伤听力
            //? 可选：应用适度增益（如需要）
            max98367a_apply_gain(passthrough_buf, bytes_read);
            
            //? 立即输出到扬声器
            if (i2s_channel_write(tx_handle, passthrough_buf, bytes_read, &bytes_written, portMAX_DELAY) != ESP_OK) {
                ESP_LOGW("AUDIO_PASSTHROUGH", "Failed to write audio");
            }
        }
        
        //? 无延迟，实时处理
    }
    vTaskDelete(NULL);
}


void app_main(void) 
{
    // 执行所有系统初始化
    ESP_ERROR_CHECK(app_init_all());
    
    
    
    // BaseType_t ret2 = xTaskCreatePinnedToCore(i2s_read_task, "i2s_read_send_task", 4096, NULL, 10, NULL, 0);
    // if (ret2 != pdPASS) 
    // {
    //     ESP_LOGE("MAIN", "Failed to create i2s_read_send_task");
    // }
    // else 
    // {
    //     ESP_LOGI("MAIN", "i2s_read_send_task created successfully");
    // }
    BaseType_t ret3 = xTaskCreatePinnedToCore(i2s_send_task, "i2s_send_task", 4096, NULL, 10, NULL, 0);
    if (ret3 != pdPASS) 
    {
        ESP_LOGE("MAIN", "Failed to create i2s_send_task");
    }
    else 
    {
        ESP_LOGI("MAIN", "i2s_send_task created successfully");
    }

    ESP_LOGI(TAG, "准备启动视频录制任务...");
    // 启动视频录制任务 (短按按键控制录制)
    // BaseType_t ret = xTaskCreatePinnedToCore(video_record_task, "video_record", 8192, NULL, 5, NULL, 1);
    // if (ret != pdPASS) {
    //     ESP_LOGE(TAG, "创建视频录制任务失败!");
    // } else {
    //     ESP_LOGI(TAG, "视频录制任务创建成功");
    // }
    // xTaskCreatePinnedToCore(lcd_show_task, "lcd_show_task", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(camera_task, "camera_task", 8192, NULL, 11, NULL, 1);

    //? 创建音频直通任务（麦克风 → 扬声器实时回放）
    // BaseType_t ret4 = xTaskCreatePinnedToCore(audio_passthrough_task, "audio_passthrough_task", 4096, NULL, 10, NULL, 1);
    // if (ret4 != pdPASS) 
    // {
    //     ESP_LOGE("MAIN", "Failed to create audio_passthrough_task");
    // }
    // else 
    // {
    //     ESP_LOGI("MAIN", "audio_passthrough_task created successfully");
    // }


}