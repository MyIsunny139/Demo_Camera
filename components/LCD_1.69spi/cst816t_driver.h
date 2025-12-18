#ifndef _CST816T_DRIVER_H_
#define _CST816T_DRIVER_H_
#include "driver/gpio.h"
#include "esp_err.h"

// LCD 引脚定义
#define LCD_RST     GPIO_NUM_19
#define LCD_MOSI    GPIO_NUM_20
#define LCD_CLK     GPIO_NUM_21
#define LCD_CS      GPIO_NUM_47
#define LCD_DC      GPIO_NUM_45

#define LCD_BL      GPIO_NUM_41

// 触摸屏引脚定义
#define TP_SDA      GPIO_NUM_4
#define TP_SCL      GPIO_NUM_5
//CST816T 触摸IC驱动

typedef struct 
{
    gpio_num_t  scl;     //SCL管脚
    gpio_num_t  sda;     //SDA管脚
    uint32_t    fre;       //I2C速率
    uint16_t    x_limit;    //X方向触摸边界
    uint16_t    y_limit;    //y方向触摸边界
}cst816t_cfg_t;


/** CST816T初始化
 * @param cfg 配置
 * @return err
*/
esp_err_t   cst816t_init(cst816t_cfg_t* cfg);

/** 读取坐标值
 * @param  x x坐标
 * @param  y y坐标
 * @param state 松手状态 0,松手 1按下
 * @return 无
*/
void cst816t_read(int16_t *x,int16_t *y,int *state);

#endif
