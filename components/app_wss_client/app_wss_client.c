/**
 * @file app_wss_client.c
 * @brief WebSocket 客户端图片上传模块实现
 */

#include <stdio.h>
#include <string.h>
#include "app_wss_client.h"
#include "esp_websocket_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "MAX98367A.h"  // 引入BUF_SIZE定义

static const char *TAG = "WSS_CLIENT";

static esp_websocket_client_handle_t s_client = NULL;
static bool s_is_connected = false;

// 外部音频播放队列 (在App_init.c中创建)
extern QueueHandle_t audio_playback_queue;

// 音频数据接收缓冲区 (用于处理WebSocket分片数据)
static uint8_t s_audio_recv_buf[BUF_SIZE];
static size_t s_audio_recv_offset = 0;

/**
 * @brief 处理接收到的PCM音频数据
 * 将音频数据分成BUF_SIZE(2048字节)的包放入播放队列
 * @param data 音频数据指针
 * @param len 数据长度
 */
static void process_audio_data(const uint8_t *data, int len)
{
    if (audio_playback_queue == NULL) {
        ESP_LOGW(TAG, "音频播放队列未创建");
        return;
    }
    
    int offset = 0;
    
    while (offset < len) {
        // 计算本次可以拷贝的字节数
        size_t copy_len = len - offset;
        size_t space_left = BUF_SIZE - s_audio_recv_offset;
        
        if (copy_len > space_left) {
            copy_len = space_left;
        }
        
        // 拷贝数据到接收缓冲区
        memcpy(s_audio_recv_buf + s_audio_recv_offset, data + offset, copy_len);
        s_audio_recv_offset += copy_len;
        offset += copy_len;
        
        // 如果缓冲区满了，发送到队列
        if (s_audio_recv_offset >= BUF_SIZE) {
            if (xQueueSend(audio_playback_queue, s_audio_recv_buf, 0) != pdPASS) {
                // 队列满，丢弃数据（优先保证实时性）
                ESP_LOGW(TAG, "音频播放队列满，丢弃数据帧");
            } else {
                ESP_LOGD(TAG, "音频帧已入队 (%d bytes)", BUF_SIZE);
            }
            s_audio_recv_offset = 0;  // 重置缓冲区偏移
        }
    }
}

/**
 * @brief WebSocket 事件回调
 */
static void websocket_event_handler(void *handler_args, esp_event_base_t base, 
                                     int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WebSocket 已连接");
        s_is_connected = true;
        s_audio_recv_offset = 0;  // 重置音频缓冲区
        break;
        
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "WebSocket 断开连接");
        s_is_connected = false;
        s_audio_recv_offset = 0;  // 重置音频缓冲区
        break;
        
    case WEBSOCKET_EVENT_DATA:
        if (data->op_code == 0x01) { // 文本消息
            ESP_LOGI(TAG, "收到消息: %.*s", data->data_len, (char *)data->data_ptr);
        } else if (data->op_code == 0x02) { // 二进制消息 (PCM音频数据)
            ESP_LOGD(TAG, "收到二进制数据: %d 字节 (payload: %d, offset: %d)", 
                     data->data_len, data->payload_len, data->payload_offset);
            // 处理PCM音频数据，分包放入播放队列
            process_audio_data((const uint8_t *)data->data_ptr, data->data_len);
        }
        break;
        
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WebSocket 错误");
        s_is_connected = false;
        break;
        
    default:
        break;
    }
}

esp_err_t wss_client_init(const char *uri)
{
    if (s_client != NULL) {
        ESP_LOGW(TAG, "WebSocket 客户端已初始化");
        return ESP_OK;
    }
    
    if (uri == NULL) {
        ESP_LOGE(TAG, "URI 不能为空");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_websocket_client_config_t ws_cfg = {
        .uri = uri,
        .buffer_size = 8192,           // 发送缓冲区大小
        .network_timeout_ms = 10000,   // 网络超时 10 秒
        .reconnect_timeout_ms = 5000,  // 重连间隔 5 秒
        .ping_interval_sec = 30,       // 心跳间隔 30 秒
    };
    
    s_client = esp_websocket_client_init(&ws_cfg);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "WebSocket 客户端初始化失败");
        return ESP_FAIL;
    }
    
    esp_websocket_register_events(s_client, WEBSOCKET_EVENT_ANY, websocket_event_handler, NULL);
    
    ESP_LOGI(TAG, "WebSocket 客户端初始化成功: %s", uri);
    return ESP_OK;
}

esp_err_t wss_client_connect(void)
{
    if (s_client == NULL) {
        ESP_LOGE(TAG, "WebSocket 客户端未初始化");
        return ESP_FAIL;
    }
    
    if (s_is_connected) {
        ESP_LOGW(TAG, "WebSocket 已连接");
        return ESP_OK;
    }
    
    esp_err_t ret = esp_websocket_client_start(s_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WebSocket 启动失败: 0x%x", ret);
        return ret;
    }
    
    // 等待连接建立 (最多等待 5 秒)
    int timeout = 50;
    while (!s_is_connected && timeout-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    if (!s_is_connected) {
        ESP_LOGW(TAG, "WebSocket 连接超时，将在后台重试");
        return ESP_ERR_TIMEOUT;
    }
    
    return ESP_OK;
}

esp_err_t wss_client_upload_image(camera_fb_t *fb)
{
    if (!s_is_connected) {
        ESP_LOGD(TAG, "WebSocket 未连接，跳过上传");
        return ESP_FAIL;
    }
    
    if (fb == NULL || fb->buf == NULL || fb->len == 0) {
        ESP_LOGE(TAG, "无效的图片数据");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGD(TAG, "上传图片: %u 字节", fb->len);
    
    // 发送二进制数据
    int ret = esp_websocket_client_send_bin(s_client, (const char *)fb->buf, fb->len, pdMS_TO_TICKS(5000));
    if (ret < 0) {
        ESP_LOGE(TAG, "图片上传失败: %d", ret);
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "图片上传成功");
    return ESP_OK;
}

esp_err_t wss_client_send_bin(const uint8_t *data, size_t len)
{
    if (!s_is_connected) {
        return ESP_FAIL;
    }
    
    int ret = esp_websocket_client_send_bin(s_client, (const char *)data, len, pdMS_TO_TICKS(5000));
    return (ret >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t wss_client_send_text(const char *text)
{
    if (!s_is_connected) {
        return ESP_FAIL;
    }
    
    int ret = esp_websocket_client_send_text(s_client, text, strlen(text), pdMS_TO_TICKS(5000));
    return (ret >= 0) ? ESP_OK : ESP_FAIL;
}

bool wss_client_is_connected(void)
{
    return s_is_connected;
}

void wss_client_disconnect(void)
{
    if (s_client != NULL) {
        esp_websocket_client_stop(s_client);
        esp_websocket_client_destroy(s_client);
        s_client = NULL;
        s_is_connected = false;
        ESP_LOGI(TAG, "WebSocket 已断开并销毁");
    }
}
