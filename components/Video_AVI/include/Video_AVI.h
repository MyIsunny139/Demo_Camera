/**
 * @file Video_AVI.h
 * @brief AVI视频录制模块接口
 * 
 * 提供基于MJPEG编码的AVI视频文件录制功能，
 * 支持按键控制开始/停止录制，LED状态指示。
 */

#ifndef _VIDEO_AVI_H_
#define _VIDEO_AVI_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================== AVI写入器接口 ====================

/**
 * @brief AVI文件句柄类型
 */
typedef struct avi_writer_s avi_writer_t;

/**
 * @brief 创建AVI写入器
 * 
 * @param filename 输出文件路径
 * @param width 视频宽度（像素）
 * @param height 视频高度（像素）
 * @param fps 帧率
 * @return avi_writer_t* 成功返回句柄，失败返回NULL
 */
avi_writer_t* avi_writer_create(const char *filename, uint16_t width, uint16_t height, uint8_t fps);

/**
 * @brief 添加视频帧
 * 
 * @param avi AVI句柄
 * @param data JPEG帧数据
 * @param size 数据大小
 * @return int 0成功，-1失败
 */
int avi_writer_add_frame(avi_writer_t *avi, const uint8_t *data, uint32_t size);

/**
 * @brief 关闭AVI文件
 * 
 * 写入索引并更新头部信息
 * 
 * @param avi AVI句柄
 * @return int 0成功，-1失败
 */
int avi_writer_close(avi_writer_t *avi);

// ==================== 视频录制器接口 ====================

/**
 * @brief 视频录制配置
 */
typedef struct {
    uint16_t width;         /**< 视频宽度 */
    uint16_t height;        /**< 视频高度 */
    uint8_t fps;            /**< 帧率 */
    uint32_t max_frames;    /**< 最大帧数（0表示使用默认值3000） */
    const char *save_path;  /**< 保存路径前缀（如"/0:/"） */
} video_recorder_config_t;

/**
 * @brief 初始化视频录制器
 * 
 * @param config 录制配置
 * @return esp_err_t ESP_OK成功
 */
esp_err_t video_recorder_init(const video_recorder_config_t *config);

/**
 * @brief 开始录制
 * 
 * @return esp_err_t ESP_OK成功
 */
esp_err_t video_recorder_start(void);

/**
 * @brief 请求停止录制
 * 
 * 非阻塞调用，设置停止标志后返回
 */
void video_recorder_request_stop(void);

/**
 * @brief 执行停止录制
 * 
 * 由录制任务调用，实际关闭文件
 * 
 * @return esp_err_t ESP_OK成功
 */
esp_err_t video_recorder_do_stop(void);

/**
 * @brief 添加视频帧
 * 
 * @param fb 摄像头帧缓冲区
 * @return esp_err_t ESP_OK成功
 */
esp_err_t video_recorder_add_frame(camera_fb_t *fb);

/**
 * @brief 获取录制状态
 * 
 * @return true 正在录制
 * @return false 未录制
 */
bool video_recorder_is_recording(void);

/**
 * @brief 检查是否收到停止请求
 * 
 * @return true 收到停止请求
 * @return false 未收到
 */
bool video_recorder_stop_requested(void);

/**
 * @brief 获取当前帧计数
 * 
 * @return uint32_t 帧数
 */
uint32_t video_recorder_get_frame_count(void);

#ifdef __cplusplus
}
#endif

#endif /* _VIDEO_AVI_H_ */