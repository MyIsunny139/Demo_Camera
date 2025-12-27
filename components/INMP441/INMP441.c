#include "INMP441.h"
#include "esp_log.h"

static const char *TAG = "INMP441";

i2s_chan_handle_t rx_handle;

//? 当前噪声门限值
static int32_t g_noise_gate_threshold = INMP441_NOISE_GATE_THRESHOLD;

void i2s_rx_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    
    //? 优化：减小dma frame num，降低延迟，提高实时性
    //? 从511改为256，在保持音质的同时减少缓冲延迟
    chan_cfg.dma_frame_num = INMP441_DMA_FRAME_NUM;
    chan_cfg.auto_clear = true;     //? 自动清除DMA缓冲区
    i2s_new_channel(&chan_cfg, NULL, &rx_handle);
 
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(INMP441_SAMPLE_RATE),
        
        // 虽然inmp441采集数据为24bit，但是仍可使用32bit来接收，中间存储过程不需考虑，只要让声音怎么进来就怎么出去即可
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(INMP441_BIT_WIDTH, INMP441_CHANNEL_MODE),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .dout = I2S_GPIO_UNUSED,
            .bclk = INMP_SCK,
            .ws = INMP_WS,
            .din = INMP_SD,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
 
    i2s_channel_init_std_mode(rx_handle, &std_cfg);
 
    i2s_channel_enable(rx_handle);
}

//? 设置噪声门限
void inmp441_set_noise_gate(int32_t threshold)
{
    if (threshold < 0) {
        threshold = 0;
    }
    g_noise_gate_threshold = threshold;
    if (threshold > 0) {
        ESP_LOGI(TAG, "Noise gate threshold set to: %ld", (long)g_noise_gate_threshold);
    } else {
        ESP_LOGI(TAG, "Noise gate disabled");
    }
}

//? 获取当前噪声门限
int32_t inmp441_get_noise_gate(void)
{
    return g_noise_gate_threshold;
}

//? 过滤音频数据中的噪声
void inmp441_filter_noise(void *data, size_t len)
{
    if (data == NULL || len == 0 || g_noise_gate_threshold == 0) {
        return;
    }
    
    int32_t *samples = (int32_t *)data;
    size_t sample_count = len / sizeof(int32_t);
    
    for (size_t i = 0; i < sample_count; i++) {
        int32_t sample = samples[i];
        
        //? 计算绝对值
        int32_t abs_sample = (sample >= 0) ? sample : -sample;
        
        //? 如果信号幅度低于门限，静音
        if (abs_sample < g_noise_gate_threshold) {
            samples[i] = 0;
        }
    }
}