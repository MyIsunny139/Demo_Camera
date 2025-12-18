/**
 * @file sdio-sdcard.c
 * @brief SDIO SD Card Driver Implementation
 */

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "include/sdio-sdcard.h"

static const char *TAG = "sd_file";

static sdmmc_card_t *s_card = NULL;

/**
* @brief SD 卡初始化w
* @param 无
* @retval esp_err_t
*/
esp_err_t sd_sdio_init(void)
{
    esp_err_t ret = ESP_OK;

    /* 挂载点/根目录 */
    const char mount_point[] = MOUNT_POINT;

    /* 文件系统挂载配置 */
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    /* 使用 SDMMC 主机 (SDIO 模式) */
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_1;
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED; 

    /* SDIO 1-bit 模式引脚配置 */
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1; // 1-bit 模式
    slot_config.clk = SDIO_CLK_GPIO_PIN; // GPIO 39
    slot_config.cmd = SDIO_CMD_GPIO_PIN; // GPIO 38 (CMD)
    slot_config.d0 = SDIO_D0_GPIO_PIN;  // GPIO 40 (D0)
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP; // 启用内部上拉

    /* 挂载文件系统 */
    ret = esp_vfs_fat_sdmmc_mount(mount_point,
                                  &host,
                                  &slot_config,
                                  &mount_config,
                                  &s_card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SD card (0x%x).", ret);
        }
        return ret;
    }

    /* 打印SD卡信息 */
    sdmmc_card_print_info(stdout, s_card);

    return ret;
}

/**
* @brief 获取 SD 卡相关信息
* @param out_total_bytes：总大小
* @param out_free_bytes：剩余大小
* @retval 无
*/
void sd_get_fatfs_usage(size_t *out_total_bytes, size_t *out_free_bytes)
{
 FATFS *fs;
 DWORD free_clusters;
 int res = f_getfree("0:", &free_clusters, &fs);
 assert(res == FR_OK);
 size_t total_sectors = (fs->n_fatent - 2) * fs->csize;
 size_t free_sectors = free_clusters * fs->csize;
 size_t sd_total = total_sectors / 1024;
 size_t sd_total_KB = sd_total * fs->ssize;
 size_t sd_free = free_sectors / 1024;
 size_t sd_free_KB = sd_free*fs->ssize;
 /* 假设总大小小于 4GiB，对于 SPI Flash 应该为 true */
 if (out_total_bytes != NULL)
 {
 *out_total_bytes = sd_total_KB;
 }
 
 if (out_free_bytes != NULL)
 {
 *out_free_bytes = sd_free_KB;
 }
}

/**
* @brief 写入文本文件
* @param filename: 文件名（相对于挂载点的路径，例如 "test.txt"）
* @param content: 要写入的字符串内容
* @retval esp_err_t ESP_OK表示成功
*/
esp_err_t sd_write_text_file(const char *filename, const char *content)
{
    if (filename == NULL || content == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    // 构建完整路径
    char filepath[128];
    snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, filename);

    // 打开文件进行写入
    FILE *f = fopen(filepath, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file %s for writing", filepath);
        return ESP_FAIL;
    }

    // 写入内容
    size_t written = fprintf(f, "%s", content);
    fclose(f);

    if (written > 0) {
        ESP_LOGI(TAG, "Successfully wrote %d bytes to %s", written, filepath);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to write to %s", filepath);
        return ESP_FAIL;
    }
}

/**
* @brief 追加内容到文本文件
* @param filename: 文件名
* @param content: 要追加的字符串内容
* @retval esp_err_t ESP_OK表示成功
*/
esp_err_t sd_append_text_file(const char *filename, const char *content)
{
    if (filename == NULL || content == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    char filepath[128];
    snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, filename);

    // 打开文件进行追加
    FILE *f = fopen(filepath, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file %s for appending", filepath);
        return ESP_FAIL;
    }

    size_t written = fprintf(f, "%s", content);
    fclose(f);

    if (written > 0) {
        ESP_LOGI(TAG, "Successfully appended %d bytes to %s", written, filepath);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to append to %s", filepath);
        return ESP_FAIL;
    }
}

/**
* @brief 读取文本文件内容
* @param filename: 文件名
* @param buffer: 存储读取内容的缓冲区
* @param buffer_size: 缓冲区大小
* @retval esp_err_t ESP_OK表示成功
*/
esp_err_t sd_read_text_file(const char *filename, char *buffer, size_t buffer_size)
{
    if (filename == NULL || buffer == NULL || buffer_size == 0) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    char filepath[128];
    snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, filename);

    FILE *f = fopen(filepath, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file %s for reading", filepath);
        return ESP_FAIL;
    }

    // 读取文件内容
    size_t bytes_read = fread(buffer, 1, buffer_size - 1, f);
    fclose(f);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';  // 添加字符串结束符
        ESP_LOGI(TAG, "Successfully read %d bytes from %s", bytes_read, filepath);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to read from %s", filepath);
        return ESP_FAIL;
    }
}

void test_sd_file_operations(void)
{
    // 测试写入文件
    const char *test_content = "Hello from ESP32-S3!\nThis is a test file.\n";
    esp_err_t ret = sd_write_text_file("test.txt", test_content);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Write test passed");
    }

    // 测试追加内容
    char time_str[64];
    snprintf(time_str, sizeof(time_str), "Time: %lld seconds\n", esp_timer_get_time() / 1000000);
    ret = sd_append_text_file("test.txt", time_str);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Append test passed");
    }

    // 测试读取文件
    char read_buffer[256];
    ret = sd_read_text_file("test.txt", read_buffer, sizeof(read_buffer));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Read test passed");
        printf("File content:\n%s\n", read_buffer);
    }
}

/**
* @brief 写入二进制文件（如JPEG图片）
* @param filename: 文件名（不包含挂载点路径，例如 "photo.jpg"）
* @param data: 二进制数据指针
* @param size: 数据大小（字节）
* @retval esp_err_t ESP_OK表示成功
*/
esp_err_t sd_write_jpeg_file(const char *filename, const uint8_t *data, size_t size)
{
    if (filename == NULL || data == NULL || size == 0) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    // 构建完整路径
    char filepath[128];
    snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, filename);

    // 打开文件进行二进制写入
    FILE *f = fopen(filepath, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file %s for writing", filepath);
        return ESP_FAIL;
    }

    // 写入二进制数据
    size_t written = fwrite(data, 1, size, f);
    fclose(f);

    if (written == size) {
        ESP_LOGI(TAG, "Successfully wrote %d bytes to %s", written, filepath);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to write complete data to %s (wrote %d/%d bytes)", filepath, written, size);
        return ESP_FAIL;
    }
}
