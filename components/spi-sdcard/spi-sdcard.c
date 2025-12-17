/**
 * @file spi-sdcard.c
 * @brief SPI SD Card Driver Implementation
 */

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "include/spi-sdcard.h"

static const char *TAG = "sd_file";


/* SD Card Commands */
#define CMD0    0       // GO_IDLE_STATE
#define CMD1    1       // SEND_OP_COND (MMC)
#define CMD8    8       // SEND_IF_COND
#define CMD9    9       // SEND_CSD
#define CMD10   10      // SEND_CID
#define CMD12   12      // STOP_TRANSMISSION
#define CMD16   16      // SET_BLOCKLEN
#define CMD17   17      // READ_SINGLE_BLOCK
#define CMD18   18      // READ_MULTIPLE_BLOCK
#define CMD23   23      // SET_BLOCK_COUNT
#define CMD24   24      // WRITE_BLOCK
#define CMD25   25      // WRITE_MULTIPLE_BLOCK
#define CMD55   55      // APP_CMD
#define CMD58   58      // READ_OCR
#define ACMD23  23      // SET_WR_BLK_ERASE_COUNT
#define ACMD41  41      // SD_SEND_OP_COND


spi_device_handle_t MY_SD_Handle;
static sdmmc_card_t *s_card = NULL;

/**
* @brief SD 卡初始化w
* @param 无
* @retval esp_err_t
*/
esp_err_t sd_spi_init(void)
{
 esp_err_t ret = ESP_OK;
 
 /* 挂载点/根目录 */
 const char mount_point[] = MOUNT_POINT;
 
 /* 文件系统挂载配置 */
 esp_vfs_fat_sdmmc_mount_config_t mount_config = {
  .format_if_mount_failed = false,  /* 如果挂载失败不重新格式化 */
  .max_files = 5,                   /* 打开文件最大数量 */
  .allocation_unit_size = 16 * 1024 /* 硬盘分区簇的大小 */
 };
 
 /* 使用 SDSPI 主机默认配置，使用 SPI2 */
 sdmmc_host_t host = SDSPI_HOST_DEFAULT();
 host.slot = SPI2_HOST;
 
 /* SD 卡引脚配置 */
 sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
 slot_config.gpio_cs = SD_NUM_CS;
 slot_config.host_id = host.slot;
 
 /* 挂载文件系统 */
 ret = esp_vfs_fat_sdspi_mount(mount_point,
                                &host,
                                &slot_config,
                                &mount_config,
                                &s_card);
 
 if (ret != ESP_OK)
 {
  if (ret == ESP_FAIL) {
   printf("Failed to mount filesystem.\n");
  } else {
   printf("Failed to initialize SD card (0x%x).\n", ret);
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
* @brief 初始化 SPI
* @param 无
* @retval 无
*/
void spi2_init(void)
{
 esp_err_t ret = 0;
 spi_bus_config_t spi_bus_conf = {0};
 /* SPI 总线配置 */
 spi_bus_conf.miso_io_num = SPI_MISO_GPIO_PIN; /* SPI_MISO 引脚 */ 
 spi_bus_conf.mosi_io_num = SPI_MOSI_GPIO_PIN; /* SPI_MOSI 引脚 */ 
 spi_bus_conf.sclk_io_num = SPI_CLK_GPIO_PIN; /* SPI_SCLK 引脚 */ 
 spi_bus_conf.quadwp_io_num = -1; /* SPI 写保护信号引脚，该引脚未使能 */ 
 spi_bus_conf.quadhd_io_num = -1; /* SPI 保持信号引脚，该引脚未使能 */ 
 spi_bus_conf.max_transfer_sz = 320 * 240 * 2;/* 配置最大传输大小，以字节为单位 */
 /* 初始化 SPI 总线 */
 ret = spi_bus_initialize(SPI2_HOST, &spi_bus_conf, SPI_DMA_CH_AUTO); 
 ESP_ERROR_CHECK(ret); /* 校验参数值 */ 
}
/**
* @brief SPI 发送命令
* @param handle : SPI 句柄
* @param cmd : 要发送命令
* @retval 无
*/
void spi2_write_cmd(spi_device_handle_t handle, uint8_t cmd)
{
 esp_err_t ret;
 spi_transaction_t t = {0};
 t.length = 8; /* 要传输的位数 一个字节 8 位 */
 t.tx_buffer = &cmd; /* 将命令填充进去 */
 ret = spi_device_polling_transmit(handle, &t); /* 开始传输 */
 ESP_ERROR_CHECK(ret); /* 一般不会有问题 */
}
/**
* @brief SPI 发送数据
* @param handle : SPI 句柄
* @param data : 要发送的数据
* @param len : 要发送的数据长度
* @retval 无
*/
void spi2_write_data(spi_device_handle_t handle, const uint8_t *data, int len)
{
 esp_err_t ret;
 spi_transaction_t t = {0};
 if (len == 0)
 {
 return; /* 长度为 0 没有数据要传输 */
 }
 t.length = len * 8; /* 要传输的位数 一个字节 8 位 */
 t.tx_buffer = data; /* 将命令填充进去 */
 ret = spi_device_polling_transmit(handle, &t); /* 开始传输 */
 ESP_ERROR_CHECK(ret); /* 一般不会有问题 */
}
/**
* @brief SPI 处理数据
* @param handle : SPI 句柄
* @param data : 要发送的数据
* @retval t.rx_data[0] : 接收到的数据
*/
uint8_t spi2_transfer_byte(spi_device_handle_t handle, uint8_t data)
{
 spi_transaction_t t;
 memset(&t, 0, sizeof(t));
 t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
 t.length = 8;
 t.tx_data[0] = data;
 spi_device_transmit(handle, &t);
 return t.rx_data[0];
 }
