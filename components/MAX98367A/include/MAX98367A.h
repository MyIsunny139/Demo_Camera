#ifndef _MAX98367A_H_
#define _MAX98367A_H_


#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"

//? MAX98357A引脚，根据自己连线修改
//? 注意：如需修改引脚配置，请直接修改此文件
#define MAX_DIN     GPIO_NUM_20
#define MAX_BCLK    GPIO_NUM_21
#define MAX_LRC     GPIO_NUM_47

//? I2S配置参数
//? 注意：音频配置在此组件头文件中管理
#define MAX98367A_SAMPLE_RATE     8000                 //? 采样率（标准CD音质）
#define MAX98367A_DMA_FRAME_NUM   256                   //? DMA缓冲帧数
#define MAX98367A_BIT_WIDTH       32                    //? 位宽
#define MAX98367A_CHANNEL_NUM     2                     //? 声道数（修正为2，匹配立体声）
#define MAX98367A_CHANNEL_MODE    I2S_SLOT_MODE_STEREO  //? 声道模式

//? buf size计算方法：根据esp32官方文档，buf size = dma frame num * 声道数 * 数据位宽 / 8
//? 优化：减小缓冲区，降低延迟，提高实时性（从511改为256帧）
#define BUF_SIZE    (MAX98367A_DMA_FRAME_NUM * MAX98367A_CHANNEL_NUM * MAX98367A_BIT_WIDTH / 8)
#define SAMPLE_RATE MAX98367A_SAMPLE_RATE  //? 保留旧定义用于兼容

//? 音量增益配置
//? 增益范围: 0.0 ~ 5.0 (0.0=静音, 1.0=原音量, 5.0=5倍音量)
#ifndef MAX98367A_DEFAULT_GAIN
#define MAX98367A_DEFAULT_GAIN    5.0f
#endif

//? 最大增益限制
#ifndef MAX98367A_MAX_GAIN
#define MAX98367A_MAX_GAIN        20.0f
#endif

extern i2s_chan_handle_t tx_handle;

//? 初始化I2S发送
void i2s_tx_init(void);

//? 设置音量增益
//? @param gain 增益值 (0.0 ~ 5.0)，1.0为原音量
void max98367a_set_gain(float gain);

//? 获取当前音量增益
//? @return 当前增益值
float max98367a_get_gain(void);

//? 应用增益到音频数据
//? @param data 音频数据缓冲区（int32_t数组）
//? @param len 数据长度（字节数）
void max98367a_apply_gain(void *data, size_t len);

#endif