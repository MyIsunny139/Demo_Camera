# ESP-Muxer 详细分析文档

## 1. ESP-Muxer 概述

**ESP-Muxer** 是 Espressif 官方提供的音视频多路复用库，用于将音频和视频数据封装到容器文件或直接流输出。

### 1.1 核心功能
- ✅ 支持音频和视频数据多路复用
- ✅ 支持多个音频和视频轨道
- ✅ 支持多种容器格式和编码
- ✅ 直接文件保存或流式回调输出
- ✅ 文件切片功能
- ✅ 自定义文件写入函数支持

---

## 2. 支持的容器和编码格式

### 2.1 容器格式
| 格式 | MP4 | TS  | FLV | WAV | CAF | OGG |
|------|-----|-----|-----|-----|-----|-----|
| 支持 | ✅  | ✅  | ✅  | ✅  | ✅  | ✅  |

### 2.2 视频编码
- **H264**: MP4, TS, FLV ✅
- **MJPEG**: MP4, TS (stream ID 6), FLV (codec ID 1) ✅

### 2.3 音频编码
| 编码 | MP4 | TS | FLV | WAV | CAF | OGG |
|------|-----|----|----|-----|-----|-----|
| PCM | ✅ | ✖ | ✅ | ✅ | ✅ | ✖ |
| AAC | ✅ | ✅ | ✅ | ✅ | ✅ | ✖ |
| MP3 | ✅ | ✅ | ✅ | ✅ | ✖ | ✖ |
| ADPCM | ✖ | ✖ | ✖ | ✅ | ✅ | ✖ |
| G711 Alaw | ✖ | ✖ | ✖ | ✅ | ✅ | ✖ |
| G711 Ulaw | ✖ | ✖ | ✖ | ✅ | ✅ | ✖ |
| AMR-NB | ✖ | ✖ | ✖ | ✅ | ✖ | ✖ |
| AMR-WB | ✖ | ✖ | ✖ | ✅ | ✖ | ✖ |
| OPUS | ✖ | ✖ | ✖ | ✖ | ✖ | ✅ |
| ALAC | ✖ | ✖ | ✖ | ✖ | ✅ | ✖ |

---

## 3. 核心数据结构

### 3.1 枚举类型

#### 容器类型 (`esp_muxer_type_t`)
```c
typedef enum {
    ESP_MUXER_TYPE_TS,      // Transport Stream
    ESP_MUXER_TYPE_MP4,     // MP4 容器
    ESP_MUXER_TYPE_FLV,     // Flash 视频格式
    ESP_MUXER_TYPE_WAV,     // WAV 音频格式
    ESP_MUXER_TYPE_CAF,     // Core Audio Format
    ESP_MUXER_TYPE_OGG,     // OGG 容器
    ESP_MUXER_TYPE_MAX,
} esp_muxer_type_t;
```

#### 视频编码 (`esp_muxer_video_codec_t`)
```c
typedef enum {
    ESP_MUXER_VDEC_NONE,
    ESP_MUXER_VDEC_MJPEG,   // Motion JPEG
    ESP_MUXER_VDEC_H264,    // H264/AVC
} esp_muxer_video_codec_t;
```

#### 音频编码 (`esp_muxer_audio_codec_t`)
```c
typedef enum {
    ESP_MUXER_ADEC_NONE,
    ESP_MUXER_ADEC_AAC,      // Advanced Audio Coding
    ESP_MUXER_ADEC_PCM,      // PCM (仅支持小端)
    ESP_MUXER_ADEC_MP3,      // MPEG-3
    ESP_MUXER_ADEC_ADPCM,    // ADPCM
    ESP_MUXER_ADEC_G711_A,   // G711 Alaw
    ESP_MUXER_ADEC_G711_U,   // G711 Ulaw
    ESP_MUXER_ADEC_AMR_NB,   // AMR Narrowband
    ESP_MUXER_ADEC_AMR_WB,   // AMR Wideband
    ESP_MUXER_ADEC_ALAC,     // Apple Lossless
    ESP_MUXER_ADEC_OPUS,     // OPUS
} esp_muxer_audio_codec_t;
```

### 3.2 配置结构

#### 基础配置 (`esp_muxer_config_t`)
```c
typedef struct {
    esp_muxer_type_t    muxer_type;          // 容器类型
    uint32_t            slice_duration;      // 文件切片时长 (ms)
    muxer_url_pattern   url_pattern;         // 文件路径回调函数
    muxer_data_callback data_cb;             // 数据输出回调 (非MP4可用)
    void*               ctx;                 // 回调上下文
    uint32_t            ram_cache_size;      // RAM 缓存大小 (推荐 16KB+)
    bool                no_key_frame_verify; // 是否禁用关键帧验证
} esp_muxer_config_t;
```

#### MP4 配置 (`mp4_muxer_config_t`)
```c
typedef struct {
    esp_muxer_config_t base_config;      // 基础配置
    bool               display_in_order; // 是否按顺序显示 (dts == pts)
    bool               moov_before_mdat; // moov 是否在 mdat 之前
} mp4_muxer_config_t;
```

#### 视频流信息 (`esp_muxer_video_stream_info_t`)
```c
typedef struct {
    esp_muxer_video_codec_t codec;               // 视频编码类型
    uint16_t                width;               // 视频宽度
    uint16_t                height;              // 视频高度
    uint8_t                 fps;                 // 帧率
    uint32_t                min_packet_duration; // 最小包时长 (ms)
    void*                   codec_spec_info;     // 编码特定信息 (H264: SPS+PPS)
    int                     spec_info_len;       // 特定信息长度
} esp_muxer_video_stream_info_t;
```

#### 音频流信息 (`esp_muxer_audio_stream_info_t`)
```c
typedef struct {
    esp_muxer_audio_codec_t codec;               // 音频编码类型
    uint8_t                 channel;             // 声道数
    uint8_t                 bits_per_sample;     // 采样位深
    uint16_t                sample_rate;         // 采样率
    uint32_t                min_packet_duration; // 最小包时长 (ms)
    void*                   codec_spec_info;     // 编码特定信息 (AAC: AudioSpecificConfig)
    int                     spec_info_len;       // 特定信息长度
} esp_muxer_audio_stream_info_t;
```

#### 视频包 (`esp_muxer_video_packet_t`)
```c
typedef struct {
    void*    data;      // 视频数据指针
    int      len;       // 数据长度
    uint32_t pts;       // 显示时间戳 (ms)
    uint32_t dts;       // 解码时间戳 (ms)
    bool     key_frame; // 是否为关键帧/I帧
} esp_muxer_video_packet_t;
```

#### 音频包 (`esp_muxer_audio_packet_t`)
```c
typedef struct {
    void*    data; // 音频数据指针
    int      len;  // 数据长度
    uint32_t pts;  // 显示时间戳 (ms)
} esp_muxer_audio_packet_t;
```

---

## 4. 核心 API 函数

### 4.1 注册函数
```c
// 注册容器类型
esp_muxer_err_t esp_muxer_reg(esp_muxer_type_t type, esp_muxer_reg_info_t* reg_info);

// 注册 MP4 容器
esp_muxer_err_t mp4_muxer_register(void);

// 注销所有已注册的容器
void esp_muxer_unreg_all(void);
```

### 4.2 生命周期函数
```c
// 打开 muxer 实例
esp_muxer_handle_t esp_muxer_open(esp_muxer_config_t* cfg, uint32_t size);

// 关闭 muxer 实例
esp_muxer_err_t esp_muxer_close(esp_muxer_handle_t muxer);
```

### 4.3 流管理函数
```c
// 添加视频流
esp_muxer_err_t esp_muxer_add_video_stream(
    esp_muxer_handle_t muxer,
    esp_muxer_video_stream_info_t* video_info,
    int* stream_index
);

// 添加音频流
esp_muxer_err_t esp_muxer_add_audio_stream(
    esp_muxer_handle_t muxer,
    esp_muxer_audio_stream_info_t* audio_info,
    int* stream_index
);
```

### 4.4 数据写入函数
```c
// 添加视频包
esp_muxer_err_t esp_muxer_add_video_packet(
    esp_muxer_handle_t muxer,
    int stream_index,
    esp_muxer_video_packet_t* packet
);

// 添加音频包
esp_muxer_err_t esp_muxer_add_audio_packet(
    esp_muxer_handle_t muxer,
    int stream_index,
    esp_muxer_audio_packet_t* packet
);
```

### 4.5 文件写入自定义
```c
// 自定义文件写入函数表
typedef struct {
    void* (*on_open)(char* path);              // 打开文件
    int (*on_write)(void* writer, void* buffer, int len); // 写入数据
    int (*on_seek)(void* writer, uint64_t pos); // 文件定位
    int (*on_close)(void* writer);             // 关闭文件
} esp_muxer_file_writer_t;

// 设置自定义文件写入
esp_muxer_err_t esp_muxer_set_file_writer(
    esp_muxer_handle_t muxer,
    esp_muxer_file_writer_t* writer
);
```

---

## 5. 应用场景和工作流程

### 5.1 JPEG 转 MP4 的实现步骤

1. **初始化 muxer**
   - 选择容器类型 (MP4)
   - 配置文件路径回调
   - 设置文件切片时长

2. **注册容器**
   - 调用 `mp4_muxer_register()`

3. **打开 muxer 实例**
   - 使用 `mp4_muxer_config_t` 配置
   - 调用 `esp_muxer_open()`

4. **添加视频流**
   - 设置视频参数 (分辨率、帧率、编码)
   - 使用 MJPEG 编码
   - 调用 `esp_muxer_add_video_stream()`

5. **处理每一帧 JPEG**
   - 读取 SD 卡中的 JPEG 文件
   - 构建 `esp_muxer_video_packet_t`
   - 调用 `esp_muxer_add_video_packet()`

6. **关闭 muxer**
   - 调用 `esp_muxer_close()`

---

## 6. 工作原理深度分析

### 6.1 多路复用流程
```
┌─────────────────┐
│ 视频流 (MJPEG)  │
└────────┬────────┘
         │
         ├─────────┐
         │         │
    ┌────v────┐  ┌─────────────┐
    │  Muxer  │──│ MP4 Container│
    │         │  │   (写入流)    │
    └────^────┘  └─────────────┘
         │
┌────────┴────────┐
│ 音频流 (可选)    │
└─────────────────┘

最终输出: .mp4 文件存储到 SD 卡
```

### 6.2 文件切片机制
- 基于时长切片: `slice_duration` 指定时长后自动创建新文件
- 文件路径回调: `url_pattern` 回调生成新文件路径
- 索引递增: `slice_idx` 参数用于文件命名

### 6.3 缓存优化
- **RAM 缓存**: 对齐文件系统块大小，提高写入效率
- **推荐大小**: 16KB 以上
- **速度测试**: 使用 `ram_cache_size` 微调获得最佳性能

---

## 7. 性能优化建议

### 7.1 缓存配置
| 缓存大小 | 适用场景 | 优点 | 缺点 |
|---------|---------|------|------|
| 0 KB    | 低端设备 | 内存占用少 | 速度最慢 |
| 16 KB   | 标准场景 | 性价比高 | - |
| 32 KB   | 高性能场景 | 速度快 | 内存占用增加 |
| 64 KB   | 流媒体服务 | 最优速度 | 内存占用大 |

### 7.2 MP4 特定优化
```c
// MP4 配置示例
mp4_muxer_config_t cfg = {
    .base_config = {
        .muxer_type = ESP_MUXER_TYPE_MP4,
        .ram_cache_size = 16 * 1024,  // 16KB 缓存
        .slice_duration = 60000,       // 60秒自动切片
        // ...
    },
    .display_in_order = true,  // 顺序显示
    .moov_before_mdat = true   // moov 在前 (支持流媒体)
};
```

---

## 8. 当前项目集成状态

### 8.1 已有组件
- ✅ `espressif__esp_muxer` - 多路复用库
- ✅ `espressif__esp32-camera` - 摄像头驱动
- ✅ `espressif__esp_jpeg` - JPEG 编码/解码
- ✅ `sdio-sdcard` - SD 卡驱动

### 8.2 项目现状
当前代码 (`Demo_Camera.c`) 中：
- 从摄像头获取 JPEG 帧
- 保存为 `img_XXX.jpg` 到 SD 卡
- **缺失**: JPEG 到 MP4 的转换逻辑

---

## 9. 实现 JPEG 转 MP4 的关键点

### 9.1 MJPEG 编码配置
```c
esp_muxer_video_stream_info_t video_info = {
    .codec = ESP_MUXER_VDEC_MJPEG,     // 使用 MJPEG
    .width = 240,
    .height = 240,
    .fps = 1,                           // 1 fps (根据实际调整)
    .min_packet_duration = 1000,        // 最小包时长
    .codec_spec_info = NULL,            // MJPEG 无需特定信息
    .spec_info_len = 0
};
```

### 9.2 关键问题处理
1. **时间戳管理**: PTS/DTS 需要顺序递增
2. **关键帧标记**: 所有 JPEG 帧都是关键帧 (`key_frame = true`)
3. **文件顺序**: SD 卡中的 JPEG 文件需要按名称排序读取
4. **内存管理**: JPEG 数据需从 SD 卡流式读取，避免一次性加载

### 9.3 推荐工作流程
```
读取 SD 卡中所有 JPEG 文件列表
    ↓
按名称排序 (img_000.jpg, img_001.jpg, ...)
    ↓
创建 MP4 Muxer 实例
    ↓
添加 MJPEG 视频流
    ↓
逐个读取 JPEG 文件
    ├─ 读取数据
    ├─ 设置 PTS (pts = frame_idx * 1000 / fps)
    ├─ 标记为关键帧
    └─ 添加到 muxer
    ↓
关闭 muxer → 生成 MP4 文件
```

---

## 10. 使用示例伪代码

```c
#include "esp_muxer.h"
#include "mp4_muxer.h"

// 文件路径回调
static int file_pattern_cb(char *file_name, int len, int slice_idx)
{
    snprintf(file_name, len, "/0:/video_%d.mp4", slice_idx);
    return 0;
}

// JPEG 转 MP4
esp_err_t jpeg_to_mp4(void)
{
    // 1. 注册 MP4 容器
    mp4_muxer_register();
    
    // 2. 配置 MP4 muxer
    mp4_muxer_config_t cfg = {
        .base_config = {
            .muxer_type = ESP_MUXER_TYPE_MP4,
            .url_pattern = file_pattern_cb,
            .slice_duration = 0xFFFFFFFF,  // 不自动切片
            .ram_cache_size = 16 * 1024,
        },
        .display_in_order = true,
        .moov_before_mdat = true,
    };
    
    // 3. 打开 muxer
    esp_muxer_handle_t muxer = esp_muxer_open(
        (esp_muxer_config_t*)&cfg,
        sizeof(mp4_muxer_config_t)
    );
    
    // 4. 添加视频流
    esp_muxer_video_stream_info_t video_info = {
        .codec = ESP_MUXER_VDEC_MJPEG,
        .width = 240,
        .height = 240,
        .fps = 1,
        .min_packet_duration = 1000,
    };
    int video_idx = 0;
    esp_muxer_add_video_stream(muxer, &video_info, &video_idx);
    
    // 5. 逐个处理 JPEG 文件
    for (int i = 0; i < num_frames; i++) {
        // 读取 JPEG 文件
        uint8_t *jpeg_data = ...;
        int jpeg_len = ...;
        
        // 创建视频包
        esp_muxer_video_packet_t packet = {
            .data = jpeg_data,
            .len = jpeg_len,
            .pts = i * 1000 / 1,  // fps = 1
            .dts = 0,
            .key_frame = true,
        };
        
        // 添加到 muxer
        esp_muxer_add_video_packet(muxer, video_idx, &packet);
        
        free(jpeg_data);
    }
    
    // 6. 关闭 muxer
    esp_muxer_close(muxer);
    esp_muxer_unreg_all();
    
    return ESP_OK;
}
```

---

## 11. 注意事项

### 11.1 关键限制
- ❌ MP4 和 WAV 不支持流输出 (不能使用 `data_cb`)
- ❌ FLV 和 TS 通过非官方方式支持 MJPEG
- ⚠️ 需要 FFmpeg patch 来完全支持 FLV/TS 中的 MJPEG

### 11.2 内存考虑
- ESP32-S3 有 PSRAM，适合处理大文件
- 建议使用流式读取而非一次性加载
- 缓存大小需根据 SD 卡速度调优

### 11.3 性能预期
- 写入速度: 约 100KB/s - 500KB/s (取决于 SD 卡和缓存配置)
- 内存占用: 基础库 ~20KB + 缓存大小
- CPU 占用: 低 (muxing 本身 CPU 消耗小)

---

## 总结

**ESP-Muxer** 是一个强大的多路复用库，完全可以满足 JPEG 转 MP4 的需求：
1. 支持 MJPEG 编码，直接兼容 JPEG 数据
2. 提供灵活的配置和文件管理
3. 性能优化选项丰富
4. 与 ESP32 生态无缝集成

只需实现文件读取、流管理和时间戳管理的胶水代码即可。
