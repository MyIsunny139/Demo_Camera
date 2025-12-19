# Video_AVI 组件

基于 MJPEG 编码的 AVI 视频录制组件，适用于 ESP32 系列芯片。

## 功能特性

- ✅ 标准 AVI 容器格式（RIFF）
- ✅ MJPEG 视频编码（每帧为独立 JPEG）
- ✅ 通用播放器兼容（VLC、Windows Media Player 等）
- ✅ 实时录制，支持按键控制
- ✅ 自动索引生成（idx1 chunk）
- ✅ 线程安全的状态管理

## 硬件要求

- ESP32-S3 或更高（需要 PSRAM）
- 摄像头模块（支持 JPEG 输出）OV5670
- SD 卡（SDIO 或 SPI 接口）

## 依赖组件

- `esp32-camera` - 摄像头驱动
- `esp_timer` - 时间戳获取

## 快速开始

### 1. 添加组件

将 `Video_AVI` 文件夹复制到项目的 `components/` 目录：

```
your_project/
├── components/
│   └── Video_AVI/
│       ├── CMakeLists.txt
│       ├── Video_AVI.c
│       ├── Video_AVI.h
│       └── README.md
└── main/
```

### 2. 配置依赖

在 `main/CMakeLists.txt` 中添加：

```cmake
idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES Video_AVI esp32-camera
                    )
```

### 3. 代码示例

```c
#include "Video_AVI.h"
#include "esp_camera.h"

void app_main(void)
{
    // 初始化摄像头（略）
    camera_init();
    
    // 初始化录制器
    video_recorder_config_t cfg = {
        .width = 320,
        .height = 240,
        .fps = 10,
        .max_frames = 0,        // 0 = 使用默认值 3000
        .save_path = "/sdcard/" // SD 卡挂载路径
    };
    ESP_ERROR_CHECK(video_recorder_init(&cfg));
    
    // 开始录制
    ESP_ERROR_CHECK(video_recorder_start());
    
    // 录制循环
    while (video_recorder_is_recording()) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            video_recorder_add_frame(fb);
            esp_camera_fb_return(fb);
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // 控制帧率
    }
}
```

## API 参考

### 初始化

#### `video_recorder_init()`

```c
esp_err_t video_recorder_init(const video_recorder_config_t *config);
```

初始化视频录制器。

**参数：**
- `config` - 录制配置结构体指针

**配置结构：**
```c
typedef struct {
    uint16_t width;         // 视频宽度（像素）
    uint16_t height;        // 视频高度（像素）
    uint8_t fps;            // 帧率
    uint32_t max_frames;    // 最大帧数（0=默认3000）
    const char *save_path;  // 保存路径（如 "/sdcard/"）
} video_recorder_config_t;
```

**返回值：**
- `ESP_OK` - 成功
- `ESP_ERR_INVALID_ARG` - 参数错误

---

### 录制控制

#### `video_recorder_start()`

```c
esp_err_t video_recorder_start(void);
```

开始录制视频，创建 AVI 文件。文件名自动生成为 `vid_000.avi`、`vid_001.avi` 等。

**返回值：**
- `ESP_OK` - 成功
- `ESP_FAIL` - 文件创建失败
- `ESP_ERR_INVALID_STATE` - 未初始化

#### `video_recorder_request_stop()`

```c
void video_recorder_request_stop(void);
```

请求停止录制（非阻塞）。设置停止标志后立即返回。

#### `video_recorder_do_stop()`

```c
esp_err_t video_recorder_do_stop(void);
```

执行停止操作，关闭 AVI 文件。通常由录制任务在检测到停止请求后调用。

**返回值：**
- `ESP_OK` - 成功

---

### 帧操作

#### `video_recorder_add_frame()`

```c
esp_err_t video_recorder_add_frame(camera_fb_t *fb);
```

添加一帧视频数据。

**参数：**
- `fb` - 摄像头帧缓冲区（必须是 JPEG 格式）

**返回值：**
- `ESP_OK` - 成功
- `ESP_FAIL` - 添加失败
- `ESP_ERR_INVALID_ARG` - 参数错误

---

### 状态查询

#### `video_recorder_is_recording()`

```c
bool video_recorder_is_recording(void);
```

检查是否正在录制。

**返回值：**
- `true` - 正在录制
- `false` - 未录制

#### `video_recorder_stop_requested()`

```c
bool video_recorder_stop_requested(void);
```

检查是否收到停止请求。

**返回值：**
- `true` - 已收到停止请求
- `false` - 未收到

#### `video_recorder_get_frame_count()`

```c
uint32_t video_recorder_get_frame_count(void);
```

获取当前录制的帧数。

**返回值：**
- 当前帧数

---

## 底层 AVI 写入器 API

如果需要更细粒度的控制，可以直接使用底层 AVI 写入器接口：

### `avi_writer_create()`

```c
avi_writer_t* avi_writer_create(const char *filename, 
                                 uint16_t width, 
                                 uint16_t height, 
                                 uint8_t fps);
```

创建 AVI 写入器。

### `avi_writer_add_frame()`

```c
int avi_writer_add_frame(avi_writer_t *avi, 
                         const uint8_t *data, 
                         uint32_t size);
```

添加原始 JPEG 帧数据。

### `avi_writer_close()`

```c
int avi_writer_close(avi_writer_t *avi);
```

关闭 AVI 文件并写入索引。

---

## 使用场景

### 场景 1：按键控制录制

```c
// 按键短按回调
void button_press_callback(void) {
    if (!video_recorder_is_recording()) {
        video_recorder_start();
        gpio_set_level(LED_GPIO, 1); // LED 亮起
    } else {
        video_recorder_request_stop();
    }
}

// 录制任务
void record_task(void *param) {
    while (1) {
        if (video_recorder_stop_requested()) {
            video_recorder_do_stop();
            gpio_set_level(LED_GPIO, 0); // LED 熄灭
        }
        
        if (video_recorder_is_recording()) {
            camera_fb_t *fb = esp_camera_fb_get();
            if (fb) {
                video_recorder_add_frame(fb);
                esp_camera_fb_return(fb);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### 场景 2：定时录制

```c
// 录制 30 秒
video_recorder_start();

for (int i = 0; i < 300; i++) { // 300 帧 @ 10fps = 30秒
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
        video_recorder_add_frame(fb);
        esp_camera_fb_return(fb);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
}

video_recorder_request_stop();
vTaskDelay(pdMS_TO_TICKS(200)); // 等待文件关闭
video_recorder_do_stop();
```

### 场景 3：运动检测触发

```c
while (1) {
    camera_fb_t *fb = esp_camera_fb_get();
    
    if (motion_detected(fb)) {
        if (!video_recorder_is_recording()) {
            video_recorder_start(); // 开始录制
        }
    }
    
    if (video_recorder_is_recording()) {
        video_recorder_add_frame(fb);
        
        // 10 秒无运动则停止
        if (no_motion_for(10000)) {
            video_recorder_request_stop();
            video_recorder_do_stop();
        }
    }
    
    esp_camera_fb_return(fb);
    vTaskDelay(pdMS_TO_TICKS(100));
}
```

---

## 性能参数

| 参数 | 典型值 |
|------|--------|
| 分辨率 | 240x240 ~ 640x480 |
| 帧率 | 5~15 fps |
| 码率 | ~500 KB/s @ 320x240 10fps |
| 内存占用 | ~50 KB（不含帧缓冲） |
| 最大录制时长 | 5 分钟（3000 帧 @ 10fps） |

## 文件格式

生成的 AVI 文件结构：

```
RIFF ('AVI ')
├── LIST ('hdrl')
│   ├── avih (主头部)
│   └── LIST ('strl')
│       ├── strh (流头部)
│       └── strf (流格式 - BITMAPINFOHEADER)
├── LIST ('movi')
│   ├── 00dc (帧 0 - JPEG 数据)
│   ├── 00dc (帧 1 - JPEG 数据)
│   └── ...
└── idx1 (索引表)
```

## 注意事项

1. **摄像头格式**：必须配置为 `PIXFORMAT_JPEG`
2. **SD 卡速度**：建议使用 Class 10 或更高
3. **帧率限制**：实际帧率取决于摄像头性能和 SD 卡写入速度
4. **内存管理**：录制过程中会分配内存存储帧索引，最大帧数越大内存占用越多
5. **文件完整性**：必须调用 `video_recorder_do_stop()` 才能生成完整的 AVI 文件

## 故障排查

### 文件无法播放

- 检查是否调用了 `video_recorder_do_stop()`
- 确认摄像头输出格式为 JPEG
- 查看串口日志确认帧数 > 0

### 录制帧率低

- 降低分辨率或 JPEG 质量
- 检查 SD 卡写入速度
- 增大摄像头帧缓冲区数量（`fb_count`）

### 内存不足

- 降低 `max_frames` 值
- 启用 PSRAM（ESP32-S3）
- 优化其他任务内存占用

## 许可证

本组件基于 ESP-IDF 开发，遵循相同的 Apache 2.0 许可证。

## 作者

开发于 ESP32 Demo Camera 项目

## 版本历史

- **v1.0.0** (2025-12-19)
  - 初始版本
  - 支持 MJPEG+AVI 录制
  - 支持实时录制和按键控制
