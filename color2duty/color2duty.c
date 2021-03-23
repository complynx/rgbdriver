#include <color2duty.h>

//#define DEBUG true

static const uint16_t color2duty_arr[256] = {
        // 16 x 16
        0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,0x0008,0x0009,0x000a,0x000b,0x000c,0x000d,0x000e,0x000f,
        0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,0x0018,0x0019,0x001a,0x001b,0x001c,0x001d,0x001e,0x001f,
        0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,0x0028,0x002a,0x002d,0x002f,0x0032,0x0035,0x0038,0x003b,
        0x003e,0x0042,0x0045,0x0049,0x004c,0x0050,0x0054,0x0058,0x005c,0x0060,0x0065,0x0069,0x006d,0x0072,0x0077,0x007c,
        0x0081,0x0086,0x008b,0x0090,0x0096,0x009b,0x00a1,0x00a7,0x00ad,0x00b3,0x00b9,0x00c0,0x00c6,0x00cd,0x00d3,0x00da,
        0x00e1,0x00e8,0x00f0,0x00f7,0x00ff,0x0106,0x010e,0x0116,0x011e,0x0126,0x012f,0x0137,0x0140,0x0148,0x0151,0x015a,
        0x0164,0x016d,0x0176,0x0180,0x018a,0x0194,0x019e,0x01a8,0x01b2,0x01bd,0x01c8,0x01d3,0x01de,0x01e9,0x01f4,0x01ff,
        0x020b,0x0217,0x0223,0x022f,0x023b,0x0247,0x0254,0x0261,0x026e,0x027b,0x0288,0x0295,0x02a3,0x02b0,0x02be,0x02cc,
        0x02db,0x02e9,0x02f7,0x0306,0x0315,0x0324,0x0333,0x0343,0x0352,0x0362,0x0372,0x0382,0x0392,0x03a3,0x03b3,0x03c4,
        0x03d5,0x03e6,0x03f7,0x0409,0x041a,0x042c,0x043e,0x0450,0x0463,0x0475,0x0488,0x049b,0x04ae,0x04c2,0x04d5,0x04e9,
        0x04fd,0x0511,0x0525,0x0539,0x054e,0x0563,0x0578,0x058d,0x05a2,0x05b8,0x05ce,0x05e3,0x05fa,0x0610,0x0626,0x063d,
        0x0654,0x066b,0x0683,0x069a,0x06b2,0x06ca,0x06e2,0x06fa,0x0713,0x072b,0x0744,0x075d,0x0777,0x0790,0x07aa,0x07c4,
        0x07de,0x07f8,0x0813,0x082e,0x0849,0x0864,0x087f,0x089b,0x08b6,0x08d2,0x08ef,0x090b,0x0928,0x0944,0x0961,0x097f,
        0x099c,0x09ba,0x09d8,0x09f6,0x0a14,0x0a33,0x0a52,0x0a71,0x0a90,0x0aaf,0x0acf,0x0aef,0x0b0f,0x0b2f,0x0b4f,0x0b70,
        0x0b91,0x0bb2,0x0bd4,0x0bf5,0x0c17,0x0c39,0x0c5b,0x0c7e,0x0ca1,0x0cc4,0x0ce7,0x0d0a,0x0d2e,0x0d52,0x0d76,0x0d9a,
        0x0dbf,0x0de3,0x0e08,0x0e2e,0x0e53,0x0e79,0x0e9f,0x0ec5,0x0eeb,0x0f12,0x0f39,0x0f60,0x0f87,0x0faf,0x0fd6,0x0fff
};

uint8_t duty2color(uint16_t duty){
    const uint16_t *a=color2duty_arr,*b=color2duty_arr+127,*c=color2duty_arr+255;
    if(duty>=*c) return 255;
    while(1){
        if(a==b) return b-color2duty_arr;
        if(*b == duty) return b-color2duty_arr;
        if(*b > duty){
            c=b;
        }else a=b;
        b=((c-a)>>1) + a;
    }
}

uint16_t color2duty(uint8_t color){
    return color2duty_arr[color];
}

#define max3(a, b, c) (((a)>(b))?(((c)>(a))?(c):(a)):((c)>(b))?(c):(b))
#define min3(a, b, c) (((a)<(b))?(((c)<(a))?(c):(a)):((c)<(b))?(c):(b))
#define clip(a, M, m) ((a)>(M)?(M):(a)<(m)?(m):(a))

uint32_t rgbw2color(color_RGBW* rgbw){
    color_RGB c;
    c.r = clip((uint16_t)rgbw->r + rgbw->w, 255, 0);
    c.g = clip((uint16_t)rgbw->g + rgbw->w, 255, 0);
    c.b = clip((uint16_t)rgbw->b + rgbw->w, 255, 0);
#ifdef DEBUG
    c.r = rgbw->r;
    c.g = rgbw->g;
    c.b = rgbw->b;
#endif
    return RGB2COLOR(c);
}

uint32_t duty_rgbw2color(duty_RGBW* duty){
    color_RGBW c;
    c.r = duty2color(duty->r);
    c.g = duty2color(duty->g);
    c.b = duty2color(duty->b);
    c.w = duty2color(duty->w);
    return rgbw2color(&c);
}

void color2rgbw(color_RGBW* rgbw,uint32_t color){
    //Get the maximum between R, G, and B
    color_RGB c;
    float tM, multiplier, hR, hG, hB, m, Luminance;
    COLOR2RGB(c, color);
    tM = max3(c.r,c.g,c.b);

    //If the maximum value is 0, immediately return pure black.
    if(tM == 0) {
        rgbw->r = 0;
        rgbw->g = 0;
        rgbw->b = 0;
        rgbw->w = 0;
        return;
    }

    //This section serves to figure out what the color with 100% hue is
    multiplier = 255.0f / tM;
    hR = c.r * multiplier;
    hG = c.g * multiplier;
    hB = c.b * multiplier;

    //This calculates the Whiteness (not strictly speaking Luminance) of the color
    tM = max3(hR, hG, hB);
    m = min3(hR, hG, hB);
    Luminance = ((tM + m) / 2.0f - 127.5f) * (255.0f/127.5f) / multiplier;

    //Calculate the output values
    rgbw->w = clip(Luminance, 255, 0);
    rgbw->r = clip(c.r - Luminance, 255, 0);
    rgbw->g = clip(c.g - Luminance, 255, 0);
    rgbw->b = clip(c.b - Luminance, 255, 0);

#ifdef DEBUG
    rgbw->r = c.r;
    rgbw->g = c.g;
    rgbw->b = c.b;
#endif
}

void rgb2hsv(color_RGB*in, color_HSV*out)
{
	if(in == NULL || out == NULL) return;
    uint8_t min, max, delta;

    min = min3(in->r,in->g,in->b);
    max = max3(in->r,in->g,in->b);

    out->v = ((double)max)/255.; // v
    delta = max - min;
    if (delta == 0) {
        out->s = 0;
        out->h = HUE_UNDEFINED;
        return;
    }
    if( max > 0 ) { // NOTE: if Max is == 0, this divide would cause a crash
        out->s = ((double)delta / (double)max); // s
    } else {
        // if max is 0, then r = g = b = 0
        out->s = 0;
        out->h = HUE_UNDEFINED;
        return;
    }
    if( in->r == max )
        out->h = ((double)( in->g - in->b )) / (double)delta; // between yellow & magenta
    else
    if( in->g == max )
        out->h = 2.0 + ((double)( in->b - in->r )) / (double)delta;  // between cyan & yellow
    else
        out->h = 4.0 + ((double)( in->r - in->g )) / (double)delta;  // between magenta & cyan

    out->h *= 60.0;                              // to degrees

    if( out->h < 0.0 )
        out->h += 360.0; // rollover
}

void hsv2rgb(color_HSV*in, color_RGB*out)
{
	if(in == NULL || out == NULL) return;
    double      hh, p, q, t, ff;
    long        i;

    if(in->s <= 0.0) {       // < is wrong, yet for compiler
        out->r = in->v*(255); // grey
        out->g = in->v*(255);
        out->b = in->v*(255);
        return;
    }
    hh = in->h;
    if(hh >= 360.0) hh = 0.0;
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = in->v * (1.0 - in->s);
    q = in->v * (1.0 - (in->s * ff));
    t = in->v * (1.0 - (in->s * (1.0 - ff)));

    switch(i) {
    case 0:
        out->r = in->v*255;
        out->g = t*255;
        out->b = p*255;
        break;
    case 1:
        out->r = q*255;
        out->g = in->v*255;
        out->b = p*255;
        break;
    case 2:
        out->r = p*255;
        out->g = in->v*255;
        out->b = t*255;
        break;
    case 3:
        out->r = p*255;
        out->g = q*255;
        out->b = in->v*255;
        break;
    case 4:
        out->r = t*255;
        out->g = p*255;
        out->b = in->v*255;
        break;
    case 5:
    default:
        out->r = in->v*255;
        out->g = p*255;
        out->b = q*255;
        break;
    }
}


void color2duty_rgbw(duty_RGBW* rgbw,uint32_t color){
    color_RGBW c;
    color2rgbw(&c, color);
    rgbw->r = color2duty(c.r);
    rgbw->g = color2duty(c.g);
    rgbw->b = color2duty(c.b);
    rgbw->w = color2duty(c.w);
}
