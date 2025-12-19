/**
 * @file Video_AVI.c
 * @brief AVI视频录制模块实现
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/stat.h>
#include "Video_AVI.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "VIDEO_AVI";

// ==================== AVI写入器实现 ====================

// AVI文件结构体（完整定义）
struct avi_writer_s {
    FILE *file;                     // 文件句柄
    uint32_t frame_count;           // 帧数
    uint32_t movi_start;            // movi块起始位置
    uint32_t *frame_offsets;        // 帧偏移表
    uint32_t *frame_sizes;          // 帧大小表
    uint32_t max_frames;            // 最大帧数
    uint32_t total_size;            // 总数据大小
};

// 写入32位小端整数
static void write_u32_le(FILE *f, uint32_t val) {
    uint8_t buf[4] = {val & 0xFF, (val >> 8) & 0xFF, (val >> 16) & 0xFF, (val >> 24) & 0xFF};
    fwrite(buf, 1, 4, f);
}

// 写入16位小端整数
static void write_u16_le(FILE *f, uint16_t val) {
    uint8_t buf[2] = {val & 0xFF, (val >> 8) & 0xFF};
    fwrite(buf, 1, 2, f);
}

// 写入FourCC
static void write_fourcc(FILE *f, const char *fourcc) {
    fwrite(fourcc, 1, 4, f);
}

avi_writer_t* avi_writer_create(const char *filename, uint16_t width, uint16_t height, uint8_t fps) {
    avi_writer_t *avi = calloc(1, sizeof(avi_writer_t));
    if (!avi) return NULL;
    
    avi->max_frames = 3000;  // 最大5分钟 @ 10fps
    avi->frame_offsets = calloc(avi->max_frames, sizeof(uint32_t));
    avi->frame_sizes = calloc(avi->max_frames, sizeof(uint32_t));
    if (!avi->frame_offsets || !avi->frame_sizes) {
        free(avi->frame_offsets);
        free(avi->frame_sizes);
        free(avi);
        return NULL;
    }
    
    avi->file = fopen(filename, "wb");
    if (!avi->file) {
        free(avi->frame_offsets);
        free(avi->frame_sizes);
        free(avi);
        return NULL;
    }
    
    // === 写入AVI头部 (预留空间，最后更新) ===
    // RIFF header
    write_fourcc(avi->file, "RIFF");
    write_u32_le(avi->file, 0);  // 文件大小占位
    write_fourcc(avi->file, "AVI ");
    
    // hdrl LIST
    write_fourcc(avi->file, "LIST");
    write_u32_le(avi->file, 192);  // hdrl大小
    write_fourcc(avi->file, "hdrl");
    
    // avih (主AVI头)
    write_fourcc(avi->file, "avih");
    write_u32_le(avi->file, 56);  // avih结构大小
    write_u32_le(avi->file, 1000000 / fps);  // 微秒/帧
    write_u32_le(avi->file, 0);  // 最大字节率
    write_u32_le(avi->file, 0);  // 填充粒度
    write_u32_le(avi->file, 0x10);  // 标志 (AVIF_HASINDEX)
    write_u32_le(avi->file, 0);  // 总帧数占位
    write_u32_le(avi->file, 0);  // 初始帧
    write_u32_le(avi->file, 1);  // 流数量
    write_u32_le(avi->file, 0);  // 建议缓冲区大小
    write_u32_le(avi->file, width);  // 宽度
    write_u32_le(avi->file, height);  // 高度
    write_u32_le(avi->file, 0);  // 保留
    write_u32_le(avi->file, 0);
    write_u32_le(avi->file, 0);
    write_u32_le(avi->file, 0);
    
    // strl LIST (流列表)
    write_fourcc(avi->file, "LIST");
    write_u32_le(avi->file, 116);  // strl大小
    write_fourcc(avi->file, "strl");
    
    // strh (流头)
    write_fourcc(avi->file, "strh");
    write_u32_le(avi->file, 56);  // strh结构大小
    write_fourcc(avi->file, "vids");  // 视频流
    write_fourcc(avi->file, "MJPG");  // MJPEG编码
    write_u32_le(avi->file, 0);  // 标志
    write_u16_le(avi->file, 0);  // 优先级
    write_u16_le(avi->file, 0);  // 语言
    write_u32_le(avi->file, 0);  // 初始帧
    write_u32_le(avi->file, 1);  // 刻度
    write_u32_le(avi->file, fps);  // 速率
    write_u32_le(avi->file, 0);  // 开始
    write_u32_le(avi->file, 0);  // 长度占位
    write_u32_le(avi->file, 0);  // 建议缓冲区
    write_u32_le(avi->file, 10000);  // 质量
    write_u32_le(avi->file, 0);  // 采样大小
    write_u16_le(avi->file, 0);  // 左
    write_u16_le(avi->file, 0);  // 上
    write_u16_le(avi->file, width);  // 右
    write_u16_le(avi->file, height);  // 下
    
    // strf (流格式 - BITMAPINFOHEADER)
    write_fourcc(avi->file, "strf");
    write_u32_le(avi->file, 40);  // strf结构大小
    write_u32_le(avi->file, 40);  // BITMAPINFOHEADER大小
    write_u32_le(avi->file, width);  // 宽度
    write_u32_le(avi->file, height);  // 高度
    write_u16_le(avi->file, 1);  // 平面数
    write_u16_le(avi->file, 24);  // 位深
    write_fourcc(avi->file, "MJPG");  // 压缩类型
    write_u32_le(avi->file, width * height * 3);  // 图像大小
    write_u32_le(avi->file, 0);  // X像素/米
    write_u32_le(avi->file, 0);  // Y像素/米
    write_u32_le(avi->file, 0);  // 使用的颜色数
    write_u32_le(avi->file, 0);  // 重要颜色数
    
    // movi LIST
    write_fourcc(avi->file, "LIST");
    write_u32_le(avi->file, 0);  // movi大小占位
    write_fourcc(avi->file, "movi");
    
    avi->movi_start = ftell(avi->file);
    avi->frame_count = 0;
    avi->total_size = 0;
    
    ESP_LOGI(TAG, "AVI文件创建成功: %s", filename);
    return avi;
}

int avi_writer_add_frame(avi_writer_t *avi, const uint8_t *data, uint32_t size) {
    if (!avi || !avi->file || avi->frame_count >= avi->max_frames) {
        return -1;
    }
    
    // 记录帧位置
    uint32_t frame_start = ftell(avi->file);
    avi->frame_offsets[avi->frame_count] = frame_start - avi->movi_start;
    avi->frame_sizes[avi->frame_count] = size;
    
    // 写入帧数据块
    write_fourcc(avi->file, "00dc");  // 视频流0，压缩数据
    write_u32_le(avi->file, size);
    fwrite(data, 1, size, avi->file);
    
    // JPEG数据需要2字节对齐
    if (size & 1) {
        uint8_t pad = 0;
        fwrite(&pad, 1, 1, avi->file);
    }
    
    avi->frame_count++;
    avi->total_size += 8 + size + (size & 1);
    
    return 0;
}

int avi_writer_close(avi_writer_t *avi) {
    if (!avi || !avi->file) {
        return -1;
    }
    
    uint32_t movi_size = ftell(avi->file) - avi->movi_start + 4;  // +4 for "movi"
    
    // 写入索引 (idx1)
    write_fourcc(avi->file, "idx1");
    write_u32_le(avi->file, avi->frame_count * 16);
    
    for (uint32_t i = 0; i < avi->frame_count; i++) {
        write_fourcc(avi->file, "00dc");
        write_u32_le(avi->file, 0x10);  // AVIIF_KEYFRAME
        write_u32_le(avi->file, avi->frame_offsets[i]);
        write_u32_le(avi->file, avi->frame_sizes[i]);
    }
    
    uint32_t file_size = ftell(avi->file) - 8;
    
    // 更新RIFF大小
    fseek(avi->file, 4, SEEK_SET);
    write_u32_le(avi->file, file_size);
    
    // 更新总帧数 (avih中的dwTotalFrames)
    fseek(avi->file, 48, SEEK_SET);
    write_u32_le(avi->file, avi->frame_count);
    
    // 更新流长度 (strh中的dwLength)
    fseek(avi->file, 140, SEEK_SET);
    write_u32_le(avi->file, avi->frame_count);
    
    // 更新movi LIST大小
    fseek(avi->file, avi->movi_start - 8, SEEK_SET);
    write_u32_le(avi->file, movi_size);
    
    fclose(avi->file);
    avi->file = NULL;
    
    ESP_LOGI(TAG, "AVI文件已关闭, 帧数: %"PRIu32, avi->frame_count);
    
    free(avi->frame_offsets);
    free(avi->frame_sizes);
    free(avi);
    
    return 0;
}

// ==================== 视频录制器实现 ====================

// 视频录制状态结构体
typedef struct {
    volatile bool is_recording;     // 是否正在录制
    volatile bool stop_requested;   // 请求停止标志
    avi_writer_t *avi;              // AVI写入器
    uint32_t frame_count;           // 帧计数
    uint32_t start_time_ms;         // 开始时间
    uint16_t video_file_idx;        // 视频文件编号
    video_recorder_config_t config; // 配置
    bool initialized;               // 是否已初始化
} video_recorder_state_t;

static video_recorder_state_t g_recorder = {
    .is_recording = false,
    .stop_requested = false,
    .avi = NULL,
    .frame_count = 0,
    .start_time_ms = 0,
    .video_file_idx = 0,
    .initialized = false
};

esp_err_t video_recorder_init(const video_recorder_config_t *config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    g_recorder.config.width = config->width;
    g_recorder.config.height = config->height;
    g_recorder.config.fps = config->fps;
    g_recorder.config.max_frames = (config->max_frames > 0) ? config->max_frames : 3000;
    g_recorder.config.save_path = (config->save_path != NULL) ? config->save_path : "/0:/";
    g_recorder.initialized = true;
    
    ESP_LOGI(TAG, "视频录制器初始化: %dx%d @ %dfps", 
             g_recorder.config.width, g_recorder.config.height, g_recorder.config.fps);
    
    return ESP_OK;
}

esp_err_t video_recorder_start(void) {
    if (!g_recorder.initialized) {
        ESP_LOGE(TAG, "视频录制器未初始化");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_recorder.is_recording) {
        ESP_LOGW(TAG, "已经在录制中");
        return ESP_OK;
    }
    
    // 重置停止请求标志
    g_recorder.stop_requested = false;
    
    // 生成文件名
    char filename[64];
    snprintf(filename, sizeof(filename), "%svid_%03d.avi", 
             g_recorder.config.save_path, g_recorder.video_file_idx);
    ESP_LOGI(TAG, "视频文件: %s", filename);
    
    // 创建AVI写入器
    g_recorder.avi = avi_writer_create(filename, 
                                       g_recorder.config.width, 
                                       g_recorder.config.height, 
                                       g_recorder.config.fps);
    if (g_recorder.avi == NULL) {
        ESP_LOGE(TAG, "创建AVI文件失败");
        return ESP_FAIL;
    }
    
    // 初始化录制状态
    g_recorder.frame_count = 0;
    g_recorder.start_time_ms = (uint32_t)(esp_timer_get_time() / 1000);
    g_recorder.is_recording = true;
    
    ESP_LOGI(TAG, "开始录制视频 (AVI + MJPEG)");
    
    return ESP_OK;
}

void video_recorder_request_stop(void) {
    if (g_recorder.is_recording) {
        g_recorder.stop_requested = true;
        ESP_LOGI(TAG, "请求停止录制...");
    }
}

esp_err_t video_recorder_do_stop(void) {
    g_recorder.is_recording = false;
    
    // 关闭AVI文件
    if (g_recorder.avi != NULL) {
        ESP_LOGI(TAG, "正在关闭AVI文件...");
        avi_writer_close(g_recorder.avi);
        g_recorder.avi = NULL;
    }
    
    uint32_t duration_ms = (uint32_t)(esp_timer_get_time() / 1000) - g_recorder.start_time_ms;
    ESP_LOGI(TAG, "========== 停止录制 ==========");
    ESP_LOGI(TAG, "帧数: %" PRIu32 ", 时长: %.1f 秒", g_recorder.frame_count, duration_ms / 1000.0f);
    
    // 文件编号递增
    g_recorder.video_file_idx++;
    g_recorder.stop_requested = false;
    
    return ESP_OK;
}

esp_err_t video_recorder_add_frame(camera_fb_t *fb) {
    if (!g_recorder.is_recording || g_recorder.avi == NULL) {
        return ESP_OK;
    }
    
    if (fb == NULL || fb->len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 添加帧到AVI
    int ret = avi_writer_add_frame(g_recorder.avi, fb->buf, fb->len);
    if (ret != 0) {
        ESP_LOGE(TAG, "添加视频帧失败");
        return ESP_FAIL;
    }
    
    g_recorder.frame_count++;
    
    // 第一帧打印日志
    if (g_recorder.frame_count == 1) {
        ESP_LOGI(TAG, "第一帧已添加, 大小=%d", fb->len);
    }
    
    // 每10帧打印一次进度
    if (g_recorder.frame_count % 10 == 0) {
        uint32_t elapsed_ms = (uint32_t)(esp_timer_get_time() / 1000) - g_recorder.start_time_ms;
        float fps = (elapsed_ms > 0) ? ((float)g_recorder.frame_count / (elapsed_ms / 1000.0f)) : 0;
        ESP_LOGI(TAG, "录制中: %" PRIu32 " 帧, %.1f秒, FPS:%.1f", 
                 g_recorder.frame_count, elapsed_ms / 1000.0f, fps);
    }
    
    return ESP_OK;
}

bool video_recorder_is_recording(void) {
    return g_recorder.is_recording;
}

bool video_recorder_stop_requested(void) {
    return g_recorder.stop_requested;
}

uint32_t video_recorder_get_frame_count(void) {
    return g_recorder.frame_count;
}
