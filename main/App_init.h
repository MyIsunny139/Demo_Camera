#ifndef APP_INIT_H
#define APP_INIT_H

#include "esp_err.h"

/**
 * @brief 初始化按键
 */
void app_button_init(void);

/**
 * @brief 初始化摄像头
 */
void camera_init(void);

/**
 * @brief 初始化LCD显示屏
 */
void lcd_init(void);

/**
 * @brief 初始化视频录制器
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t app_video_recorder_init(void);

/**
 * @brief 初始化WebSocket客户端
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t app_wss_client_init(void);

/**
 * @brief 执行所有系统初始化
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t app_init_all(void);

#endif // APP_INIT_H
