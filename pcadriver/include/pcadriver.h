#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef uint16_t pwm_duty;

#define I2C_DEV 0

#define R_MODE1         0x00
#define R_MODE2         0x01
#define R_SUBADDR1      0x02
#define R_SUBADDR2      0x03
#define R_SUBADDR3      0x04
#define R_ALLCALLADDR   0x05
#define R_LED0          0x06

// ON_L ON_H OFF_L OFF_H
#define R_LED_STEP      4
#define R_LED_ON_L(num)      (((num) * (R_LED_STEP)) + (R_LED0))

#define D_LED_ON(num)   (num)
#define D_LED_OFF(num)  ((num) + 2)

#define D_LED_L(num)    (num)
#define D_LED_H(num)    ((num) + 1)

#define R_LED(num,onoff,lowhigh)    (D_LED_ ## lowhigh(D_LED_ ## onoff(R_LED_ON_L(num))))
#define R_LED_ON_H(num)       (R_LED((num),ON,H))
#define R_LED_OFF_L(num)      (R_LED((num),OFF,L))
#define R_LED_OFF_H(num)      (R_LED((num),OFF,H))

#define R_ALL_LED_ON_L  0xFA
#define R_ALL_LED(onoff,lowhigh)    (D_LED_ ## lowhigh(D_LED_ ## onoff(R_ALL_LED_ON_L)))
#define R_ALL_LED_ON_H       (R_ALL_LED(ON,H))
#define R_ALL_LED_OFF_L      (R_ALL_LED(OFF,L))
#define R_ALL_LED_OFF_H      (R_ALL_LED(OFF,H))

#define R_PRE_SCALE     0xFE

#define LH(lo,hi) (((pwm_duty)(lo)&0xff)|(((pwm_duty)((hi)&0x1f))<<8))
#define L(num) ((uint8_t)((num)&0xff))
#define H(num) ((uint8_t)(((num)>>8)&0x1f))

#define H_DUTY(num) ((num)&0x0f)
#define H_IS_FULL(num) ((num)&0x10)
#define H_SET_FULL(num) ((num)|0x10)

#define IS_FULL(num) ((num)&0x1000)
#define SET_FULL(num)   ((num)|0x1000)
#define DUTY(num) ((num)&0x0fff)

#define MIN(a,b) ((a)>(b)?(b):(a))

// MODE1
#define BITNUM_RESTART   7
#define BITNUM_EXTCLK    6
#define BITNUM_AI        5
#define BITNUM_SLEEP     4
#define BITNUM_SUB1      3
#define BITNUM_SUB2      2
#define BITNUM_SUB3      1
#define BITNUM_ALLCALL   0
// MODE2
#define BITNUM_INVRT     4
#define BITNUM_OCH       3
#define BITNUM_OUTDRV    2
#define BITNUM_OUTNE1    1
#define BITNUM_OUTNE0    0

#define M(type)   (1<<(BITNUM_ ## type))

#define OSCILLATOR_CLOCK		25000000.0f
#define	PULSE_TOTAL_COUNT		4096
#define CHANNEL_COUNT           16

#define PRESCALE_MIN_VALUE		0x03
#define PRESCALE_MAX_VALUE		0xff

#include <fast_i2c.h>

typedef struct _PCADriver{
    I2C* i2c;
} PCADriver;

PCADriver* PCADriver_init(int address);
int PCADriver_clear(PCADriver*);
int PCADriver_send_init(PCADriver*);
int PCADriver_set_reset(PCADriver*);
int PCADriver_set_sleep_mode(PCADriver*, char);
char PCADriver_get_sleep_mode(PCADriver*);
int PCADriver_set_ai(PCADriver* drv);

int frequency_to_prescale(float freq);
float prescale_to_frequency(int prescale);

int PCADriver_set_frequency(PCADriver* drv, float freq);
float PCADriver_get_frequency(PCADriver* drv);
int PCADriver_get_prescale(PCADriver* drv);
int PCADriver_set_prescale(PCADriver* drv, int prescale);

int PCADriver_set_on_off(PCADriver* drv, uint8_t channel, uint16_t on, uint16_t off);
#define PCADriver_set_on_off_all(drv,duty,on,off) (PCADriver_set_on_off((drv),(R_ALL_LED_ON_L), (on), (off)))
#define PCADriver_set_duty(drv,channel,duty) (PCADriver_set_on_off((drv),(channel), 0, MIN((duty), PULSE_TOTAL_COUNT)))
#define PCADriver_set_duty_all(drv,duty) (PCADriver_set_duty((drv),(R_ALL_LED_ON_L), (duty)))
#define PCADriver_set_full_on_start(drv,channel,start) (PCADriver_set_on_off((drv),(channel), (SET_FULL(start)), (0)))
#define PCADriver_set_full_on(drv,channel) (PCADriver_set_full_on_start((drv),(channel),(0)))
#define PCADriver_set_full_off(drv,channel) (PCADriver_set_on_off((drv),(channel), (0), (SET_FULL(0))))
#define PCADriver_set_full_on_start_all(drv,start) (PCADriver_set_full_on_start((drv),(R_ALL_LED_ON_L),(start)))
#define PCADriver_set_full_on_all(drv) (PCADriver_set_full_on((drv),(R_ALL_LED_ON_L)))
#define PCADriver_set_full_off_all(drv) (PCADriver_set_full_off((drv),(R_ALL_LED_ON_L)))


int PCADriver_get_on_off(PCADriver* drv, uint8_t channel, uint16_t *on, uint16_t *off);
int PCADriver_get_duty(PCADriver* drv, uint8_t channel, uint16_t *duty);
#define PCADriver_get_on_off_all(drv,duty,on,off) (PCADriver_get_on_off((drv),(R_ALL_LED_ON_L), (on), (off)))
#define PCADriver_get_duty_all(drv,duty) (PCADriver_get_duty((drv),(R_ALL_LED_ON_L), (duty)))

#ifdef __cplusplus
}
#endif
