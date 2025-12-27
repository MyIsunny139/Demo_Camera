/**
 * @file app_wss_client.h
 * @brief WebSocket 客户端图片上传模块
 * 
 * 提供通过 WebSocket 上传 JPEG 图片到服务器的功能
 */

#ifndef APP_WSS_CLIENT_H
#define APP_WSS_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 WebSocket 客户端
 * 
 * @param uri WebSocket 服务器地址，如 "ws://192.168.1.100:8080/upload"
 * @return 
 *      - ESP_OK 成功
 *      - ESP_FAIL 失败
 */
esp_err_t wss_client_init(const char *uri);

/**
 * @brief 连接到 WebSocket 服务器
 * 
 * @return 
 *      - ESP_OK 成功
 *      - ESP_ERR_TIMEOUT 连接超时
 *      - ESP_FAIL 失败
 */
esp_err_t wss_client_connect(void);

/**
 * @brief 上传 JPEG 图片
 * 
 * @param fb 摄像头帧缓冲区
 * @return 
 *      - ESP_OK 成功
 *      - ESP_FAIL 失败
 */
esp_err_t wss_client_upload_image(camera_fb_t *fb);

/**
 * @brief 上传原始二进制数据
 * 
 * @param data 数据指针
 * @param len 数据长度
 * @return 
 *      - ESP_OK 成功
 *      - ESP_FAIL 失败
 */
esp_err_t wss_client_send_bin(const uint8_t *data, size_t len);

/**
 * @brief 发送文本消息
 * 
 * @param text 文本内容
 * @return 
 *      - ESP_OK 成功
 *      - ESP_FAIL 失败
 */
esp_err_t wss_client_send_text(const char *text);

/**
 * @brief 检查连接状态
 * 
 * @return true 已连接，false 未连接
 */
bool wss_client_is_connected(void);

/**
 * @brief 断开 WebSocket 连接
 */
void wss_client_disconnect(void);

#ifdef __cplusplus
}
#endif

#endif // APP_WSS_CLIENT_H
