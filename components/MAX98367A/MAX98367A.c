#include "MAX98367A.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "MAX98367A";

i2s_chan_handle_t tx_handle;

//? 当前音量增益值
static float g_volume_gain = MAX98367A_DEFAULT_GAIN;


void i2s_tx_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    
    //? 优化：减小dma frame num，降低延迟，提高实时性
    chan_cfg.dma_frame_num = MAX98367A_DMA_FRAME_NUM;
    chan_cfg.auto_clear = true;     //? 自动清除DMA缓冲区
    i2s_new_channel(&chan_cfg, &tx_handle, NULL);
 
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(MAX98367A_SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(MAX98367A_BIT_WIDTH, MAX98367A_CHANNEL_MODE),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .din = I2S_GPIO_UNUSED,
            .bclk = MAX_BCLK,
            .ws = MAX_LRC,
            .dout = MAX_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
 
    i2s_channel_init_std_mode(tx_handle, &std_cfg);
    i2s_channel_enable(tx_handle);
}

//? 设置音量增益
void max98367a_set_gain(float gain)
{
    if (gain < 0.0f) {
        gain = 0.0f;
    } else if (gain > MAX98367A_MAX_GAIN) {
        gain = MAX98367A_MAX_GAIN;
    }
    g_volume_gain = gain;
    ESP_LOGI(TAG, "Volume gain set to: %.2f", g_volume_gain);
}

//? 获取当前音量增益
float max98367a_get_gain(void)
{
    return g_volume_gain;
}

//? 应用增益到音频数据
void max98367a_apply_gain(void *data, size_t len)
{
    if (data == NULL || len == 0) {
        return;
    }
    
    //? 如果增益为1.0，无需处理
    if (fabsf(g_volume_gain - 1.0f) < 0.01f) {
        return;
    }
    
    int32_t *samples = (int32_t *)data;
    size_t sample_count = len / sizeof(int32_t);
    
    for (size_t i = 0; i < sample_count; i++) {
        //? 应用增益
        int64_t temp = (int64_t)samples[i] * (int64_t)(g_volume_gain * 256.0f);
        temp = temp >> 8;  //? 除以256恢复
        
        //? 防止溢出
        if (temp > INT32_MAX) {
            temp = INT32_MAX;
        } else if (temp < INT32_MIN) {
            temp = INT32_MIN;
        }
        
        samples[i] = (int32_t)temp;
    }
}