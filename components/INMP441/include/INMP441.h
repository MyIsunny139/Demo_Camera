#ifndef _INMP441_H_
#define _INMP441_H_
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"

//? INMP441引脚配置，根据自己连线修改
//? 注意：如需修改引脚配置，请直接修改此文件
//? 注意：避免与摄像头引脚冲突 (GPIO 4,5,6,7,8,9,10,11,12,13,15,18 已被摄像头占用)
#define INMP_SD     GPIO_NUM_42
#define INMP_SCK    GPIO_NUM_41
#define INMP_WS     GPIO_NUM_3

//? I2S配置参数
//? 注意：音频配置在此组件头文件中管理
#define INMP441_SAMPLE_RATE     8000   //? 采样率（标准CD音质，与扬声器匹配）
#define INMP441_DMA_FRAME_NUM   256     //? DMA缓冲帧数
#define INMP441_BIT_WIDTH       32      //? 位宽
#define INMP441_CHANNEL_MODE    I2S_SLOT_MODE_STEREO  //? 声道模式

//? 噪声门限配置（用于过滤麦克风小信号杂音）
//? 低于此阈值的音频信号将被静音
//? 范围: 0 ~ INT32_MAX，建议值: 100000 ~ 10000000
//? 0 = 禁用噪声门限
#ifndef INMP441_NOISE_GATE_THRESHOLD
#define INMP441_NOISE_GATE_THRESHOLD    500000
#endif

extern i2s_chan_handle_t rx_handle;

void i2s_rx_init(void);

//? 设置噪声门限
//? @param threshold 噪声门限值，0表示禁用
void inmp441_set_noise_gate(int32_t threshold);

//? 获取当前噪声门限
//? @return 当前噪声门限值
int32_t inmp441_get_noise_gate(void);

//? 过滤音频数据中的噪声
//? @param data 音频数据缓冲区（int32_t数组）
//? @param len 数据长度（字节数）
void inmp441_filter_noise(void *data, size_t len);

#endif