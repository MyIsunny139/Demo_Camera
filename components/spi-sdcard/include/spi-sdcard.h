/**
 * @file spi-sdcard.h
 * @brief Simple SPI SD Card Driver Interface
 */

#ifndef SPI_SDCARD_H
#define SPI_SDCARD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_log.h"


#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"



/* 引脚定义 */
#define SPI_MOSI_GPIO_PIN GPIO_NUM_39 /* SPI2_MOSI */
#define SPI_CLK_GPIO_PIN GPIO_NUM_40 /* SPI2_CLK */
#define SPI_MISO_GPIO_PIN GPIO_NUM_38 /* SPI2_MISO */



/* 引脚定义 */
#define SD_NUM_CS GPIO_NUM_41 /* SPI2_CS */
#define MOUNT_POINT "/0:"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SD卡初始化
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t sd_spi_init(void);

/**
 * @brief 获取SD卡空间信息
 * @param out_total_bytes 总容量（KB）
 * @param out_free_bytes 剩余容量（KB）
 */
void sd_get_fatfs_usage(size_t *out_total_bytes, size_t *out_free_bytes);

/**
 * @brief 初始化SPI总线
 */
void spi2_init(void);

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
 * @brief 测试SD卡文件操作
 */
void test_sd_file_operations(void);
#ifdef __cplusplus
}
#endif

#endif /* SPI_SDCARD_H */
