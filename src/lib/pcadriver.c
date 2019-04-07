#include <pcadriver.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>


PCADriver* PCADriver_init(int address){
    PCADriver * ret = calloc(1, sizeof(PCADriver));
    ret->i2c = i2c_init(I2C_DEV, address);
    if(!ret->i2c){
        PCADriver_clear(ret);
        return NULL;
    }
    return ret;
}

int PCADriver_clear(PCADriver *drv){
    if(drv->i2c){
        i2c_clear(drv->i2c);
    }
    free(drv);
    return EXIT_SUCCESS;
}


int PCADriver_send_init(PCADriver* drv){
    I_ASSERT(i2c_writeByte(drv->i2c, R_MODE2, M(OUTDRV)))
    I_ASSERT(i2c_writeByte(drv->i2c, R_MODE1, M(ALLCALL)|M(AI)))

    // waiting for oscillator
    usleep(1000);

    // I_ASSERT(PCADriver_set_sleep_mode(drv, 0))
    I_ASSERT(PCADriver_set_reset(drv))

    return EXIT_SUCCESS;
}

int PCADriver_set_ai(PCADriver* drv){
    uint8_t mode;
    I_ASSERT(i2c_readByte(drv->i2c, R_MODE1, &mode))

    mode |= M(AI);
    I_ASSERT(i2c_writeByte(drv->i2c, R_MODE1, mode))

    return EXIT_SUCCESS;
}

int PCADriver_set_sleep_mode(PCADriver* drv, char sleep_mode){
    uint8_t mode;
    I_ASSERT(i2c_readByte(drv->i2c, R_MODE1, &mode))

    if(sleep_mode){
        mode |= M(SLEEP);
    }else{
        mode &= ~M(SLEEP);
    }
    I_ASSERT(i2c_writeByte(drv->i2c, R_MODE1, mode))

    return EXIT_SUCCESS;
}

char PCADriver_get_sleep_mode(PCADriver* drv){
    uint8_t mode=0;
    i2c_readByte(drv->i2c, R_MODE1, &mode);
    return !!(mode&M(SLEEP));
}

int PCADriver_set_reset(PCADriver* drv){
    uint8_t mode;
    I_ASSERT(i2c_readByte(drv->i2c, R_MODE1, &mode))
    mode |= M(RESTART);
    I_ASSERT(i2c_writeByte(drv->i2c, R_MODE1, mode))

    return EXIT_SUCCESS;
}

int PCADriver_set_prescale(PCADriver* drv, int prescale){
    uint8_t o_prescale;
    uint8_t mode;

    if(prescale > PRESCALE_MAX_VALUE) prescale = PRESCALE_MAX_VALUE;
    else if(prescale < PRESCALE_MIN_VALUE) prescale = PRESCALE_MIN_VALUE;

    I_ASSERT(i2c_readByte(drv->i2c, R_PRE_SCALE, &o_prescale))

    if(o_prescale != prescale){
        I_ASSERT(i2c_readByte(drv->i2c, R_MODE1, &mode))
        if(!(mode & M(SLEEP))){
            I_ASSERT(i2c_writeByte(drv->i2c, R_MODE1, mode|M(SLEEP)))
        }
        I_ASSERT(i2c_writeByte(drv->i2c, R_PRE_SCALE, prescale))
        if(!(mode & M(SLEEP))){
            I_ASSERT(i2c_writeByte(drv->i2c, R_MODE1, mode))
        }
        I_ASSERT(i2c_writeByte(drv->i2c, R_MODE1, mode|M(RESTART)))
    }

    return EXIT_SUCCESS;
}

int PCADriver_get_prescale(PCADriver* drv){
    uint8_t o_prescale;

    if(i2c_readByte(drv->i2c, R_PRE_SCALE, &o_prescale)){return 0;}

    return o_prescale;
}

int frequency_to_prescale(float freq){
    return (int)round( (float)OSCILLATOR_CLOCK/((float)PULSE_TOTAL_COUNT * freq) ) - 1;
}

float prescale_to_frequency(int prescale){
    return (float)OSCILLATOR_CLOCK/(((float)prescale + 1)*(float)PULSE_TOTAL_COUNT);
}

int PCADriver_set_frequency(PCADriver* drv, float freq){
    return PCADriver_set_prescale(drv, frequency_to_prescale(freq));
}

float PCADriver_get_frequency(PCADriver* drv){
    uint8_t p = PCADriver_get_prescale(drv);
    if(!p) return 0;
    return prescale_to_frequency(p);
}

int PCADriver_set_on_off(PCADriver* drv, uint8_t channel, uint16_t on, uint16_t off){
    uint8_t buf[4];
    if(channel < CHANNEL_COUNT || channel == R_ALL_LED_ON_L){
        buf[0]=L(on);
        buf[1]=H(on);
        buf[2]=L(off);
        buf[3]=H(off);

        I_ASSERT(i2c_writeBuffer(drv->i2c,channel == R_ALL_LED_ON_L ? R_ALL_LED_ON_L : R_LED_ON_L(channel), buf, 4))

        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}

int PCADriver_get_on_off(PCADriver* drv, uint8_t channel, uint16_t *on, uint16_t *off){
    uint8_t buf[2];
    if(channel < CHANNEL_COUNT || channel == R_ALL_LED_ON_L){
        if(on){
            I_ASSERT(i2c_readByte(drv->i2c, channel == R_ALL_LED_ON_L ? R_ALL_LED_ON_L : R_LED_ON_L(channel), buf))
            I_ASSERT(i2c_readByte(drv->i2c, channel == R_ALL_LED_ON_L ? R_ALL_LED_ON_H : R_LED_ON_H(channel), buf+1))
            *on = LH(buf[0], buf[1]);
        }
        if(off){
            I_ASSERT(i2c_readByte(drv->i2c, channel == R_ALL_LED_ON_L ? R_ALL_LED_OFF_L : R_LED_OFF_L(channel), buf))
            I_ASSERT(i2c_readByte(drv->i2c, channel == R_ALL_LED_ON_L ? R_ALL_LED_OFF_H : R_LED_OFF_H(channel), buf+1))
            *off = LH(buf[0], buf[1]);
        }

        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}

int PCADriver_get_duty(PCADriver* drv, uint8_t channel, uint16_t *duty){
    uint16_t on;
    int ret = PCADriver_get_on_off(drv, channel, &on, duty);
    if(IS_FULL(*duty)) *duty = 0;
    else if(IS_FULL(on)) *duty = 0x1000;
    else *duty -= on;
    return ret;
}
