/**
 * @file sdio-sdcard.h
 * @brief Simple SDIO SD Card Driver Interface
 */

#ifndef SDIO_SDCARD_H
#define SDIO_SDCARD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_log.h"


#include "driver/gpio.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

/* 引脚定义 */
#define SDIO_CMD_GPIO_PIN GPIO_NUM_38 /* SD_CMD */
#define SDIO_CLK_GPIO_PIN GPIO_NUM_39 /* SD_CLK */
#define SDIO_D0_GPIO_PIN  GPIO_NUM_40 /* SD_DATA (D0) */

#define MOUNT_POINT "/0:"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SD卡初始化
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t sd_sdio_init(void);

/**
 * @brief 获取SD卡空间信息
 * @param out_total_bytes 总容量（KB）
 * @param out_free_bytes 剩余容量（KB）
 */
void sd_get_fatfs_usage(size_t *out_total_bytes, size_t *out_free_bytes);

/**
 * @brief 写入文本文件
 * @param filename 文件名（不包含挂载点路径）
 * @param content 要写入的字符串内容
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t sd_write_text_file(const char *filename, const char *content);

/**
 * @brief 追加内容到文本文件
 * @param filename 文件名
 * @param content 要追加的字符串内容
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t sd_append_text_file(const char *filename, const char *content);

/**
 * @brief 读取文本文件内容
 * @param filename 文件名
 * @param buffer 存储读取内容的缓冲区
 * @param buffer_size 缓冲区大小
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t sd_read_text_file(const char *filename, char *buffer, size_t buffer_size);

/**
 * @brief 写入二进制文件（如JPEG图片）
 * @param filename 文件名（不包含挂载点路径）
 * @param data 二进制数据指针
 * @param size 数据大小（字节）
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t sd_write_jpeg_file(const char *filename, const uint8_t *data, size_t size);

/**
 * @brief 测试SD卡文件操作
 */
void test_sd_file_operations(void);
#ifdef __cplusplus
}
#endif

#endif /* SPI_SDCARD_H */
