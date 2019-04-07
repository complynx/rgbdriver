#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct color_rgbw_{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
} color_RGBW;

typedef struct duty_rgbw_{
    uint16_t r;
    uint16_t g;
    uint16_t b;
    uint16_t w;
} duty_RGBW;

typedef struct color_rgb_{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} color_RGB;

#define COLOR2RGB(rgb, color) ((rgb).b=((color))&0xff,(rgb).g=((color)>>8)&0xff,(rgb).r=((color)>>16)&0xff)
#define RGB2COLOR(rgb) ((uint32_t)((rgb).b & 0xff) + ((uint32_t)((rgb).g & 0xff) << 8) + ((uint32_t)((rgb).r & 0xff) << 16))

uint8_t duty2color(uint16_t duty);
uint16_t color2duty(uint8_t color);

uint32_t duty_rgbw2color(duty_RGBW *duty);
uint32_t rgbw2color(color_RGBW *rgbw);

void color2rgbw(color_RGBW* rgbw,uint32_t color);
void color2duty_rgbw(duty_RGBW* rgbw,uint32_t color);

#ifdef __cplusplus
}
#endif

