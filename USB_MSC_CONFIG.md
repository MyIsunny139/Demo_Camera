# USB U盘模式配置指南

## 功能说明

本项目已集成 USB MSC（大容量存储设备）功能，可将 SD 卡导出为 USB U 盘，方便 PC 直接读取录制的视频文件。

## 使用方法

### 1. 启用 USB MSC 功能

通过 menuconfig 启用必要的配置：

```bash
idf.py menuconfig
```

进入以下菜单项：

#### (1) 启用 USB MSC 支持

```
Component config → TinyUSB Stack → Use TinyUSB Stack → [*]
Component config → TinyUSB Stack → TinyUSB Device Classes → Enable TinyUSB MSC → [*]
```

#### (2) 配置 USB 描述符（可选）

```
Component config → TinyUSB Stack → Descriptor configuration
  - USB Device String Manufacturer: "Espressif"
  - USB Device String Product: "ESP32-S3 Camera"
  - USB Device String Serial Number: "123456"
```

#### (3) 配置 USB PHY（已自动配置）

ESP32-S3 使用内部 USB PHY，无需额外配置。

### 2. 操作流程

#### 录制视频
1. **短按按键（GPIO1）** → 开始录制，LED 常亮
2. 摄像头录制 240x240 视频到 SD 卡（`/0:/vid_000.avi`）
3. **再次短按** → 停止录制，LED 熄灭

#### 读取视频（USB U盘模式）
4. **长按按键（GPIO1，3秒）** → 启动 USB U 盘模式
   - LED 快闪 3 次（表示正在初始化）
   - 初始化成功后 LED 慢闪 1 次
5. **连接 USB 数据线到 PC**
   - PC 会识别为 USB 大容量存储设备（U 盘）
   - 可直接访问 SD 卡内容
   - 双击 `vid_xxx.avi` 播放视频

#### 退出 USB 模式
6. **PC 端安全弹出 U 盘**
7. **重启设备** 恢复正常录制模式

### 3. 注意事项

⚠️ **重要警告：**

- **录制期间禁止进入 USB 模式**：录制时长按按键会提示"录制中，无法切换USB模式"
- **USB 模式下禁止录制**：USB 模式启用后，SD 卡文件系统被 USB 占用
- **安全弹出**：从 PC 访问完毕后，务必先"安全删除硬件"再拔线
- **重启退出**：USB 模式一旦启用，需重启设备才能退出

### 4. LED 指示

| LED 状态 | 含义 |
|---------|------|
| 常亮 | 正在录制视频 |
| 快闪 3 次 | 正在初始化 USB 模式 |
| 慢闪 1 次 | USB 模式启动成功 |
| 熄灭 | 空闲状态 |

## 硬件连接

### USB 连接方式

ESP32-S3 有两个 USB 接口：

1. **USB Serial/JTAG 接口（仅调试用）**
   - 用于烧录和串口调试
   - 不支持 USB MSC 功能

2. **USB OTG 接口（用于 U 盘功能）**
   - GPIO19: USB D-
   - GPIO20: USB D+
   - 需使用 **USB OTG 接口** 连接 PC

### 接线图

```
ESP32-S3                PC
┌────────┐          ┌──────┐
│ GPIO19 │◄────────►│ USB  │
│ (D-)   │          │ Data │
│ GPIO20 │◄────────►│ Port │
│ (D+)   │          │      │
│ GND    │──────────│ GND  │
└────────┘          └──────┘
```

## 技术实现

### 代码调用流程

```c
// 1. SD 卡初始化（app_main）
ESP_ERROR_CHECK(sd_sdio_init());

// 2. 长按按键触发 USB 初始化
usb_msc_init() {
    // 初始化 TinyUSB 驱动
    tinyusb_driver_install(&tusb_cfg);
    
    // 获取 SD 卡句柄
    sdmmc_card_t *card = sd_get_card_handle();
    
    // 创建 MSC 存储
    tinyusb_msc_new_storage_sdmmc(&msc_cfg, &storage_handle);
}
```

### 文件结构

```
main/
├── Demo_Camera.c         # USB 初始化和控制逻辑
├── CMakeLists.txt        # 添加 esp_tinyusb 依赖
└── idf_component.yml     # TinyUSB 组件版本

components/
└── sdio-sdcard/
    ├── sdio-sdcard.c     # SD 卡驱动，新增 sd_get_card_handle()
    └── include/
        └── sdio-sdcard.h # 导出 SD 卡句柄接口
```

## 故障排查

### PC 无法识别 U 盘

**可能原因：**
1. USB 线连接错误（连到了 Serial/JTAG 口）
2. menuconfig 未启用 MSC
3. SD 卡未正确初始化

**解决方法：**
- 检查 USB 连接到 GPIO19/20（OTG 接口）
- 运行 `idf.py menuconfig` 检查配置
- 查看串口日志确认 SD 卡初始化成功

### U 盘显示为 RAW 格式

**原因：** SD 卡文件系统损坏

**解决方法：**
- 重新格式化 SD 卡为 FAT32
- 确保录制时正常停止（不要强行断电）

### 录制和 USB 模式冲突

**现象：** 进入 USB 模式后无法录制

**说明：** 这是正常的，USB MSC 会独占 SD 卡访问权限

**解决：** 重启设备退出 USB 模式

## 扩展功能

可以进一步实现：

1. **自动检测 USB 连接**
   - 监测 VBUS 电压
   - 插入 USB 时自动进入 U 盘模式

2. **双模式切换**
   - 实现录制和 USB 模式的热切换
   - 需要卸载/重新挂载文件系统

3. **USB 状态指示**
   - 读写操作时 LED 闪烁
   - 显示数据传输进度

## 参考文档

- [ESP-IDF TinyUSB 文档](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/usb_device.html)
- [TinyUSB 官方文档](https://docs.tinyusb.org/)
- ESP32-S3 USB OTG 硬件设计指南
