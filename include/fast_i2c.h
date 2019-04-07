#ifndef _ONION_I2C_H_
#define _ONION_I2C_H_

#include <stdlib.h>
#include <unistd.h>

#ifndef __APPLE__
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#endif


#include <sys/ioctl.h>
#include <fcntl.h>

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>


#define I2C_DEV_PATH		"/dev/i2c-%d"

#define I2C_BUFFER_SIZE		32

#define I2C_DEFAULT_ADAPTER	0

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

#define ASSERT(expr) if(!(expr)){return EXIT_FAILURE;}
#define I_ASSERT(expr) ASSERT((expr)==EXIT_SUCCESS)
#define S_ASSERT(expr) ASSERT((expr)>=0)

#define P_ASSERT(dev, expr) if(!(expr)){if(!dev->exclusive)_i2c_close_pointer(dev);return EXIT_FAILURE;}
#define PI_ASSERT(dev, expr) P_ASSERT((dev), (expr)==EXIT_SUCCESS)
#define PS_ASSERT(dev, expr) P_ASSERT((dev), (expr)>=0)


// for debugging
#ifndef __APPLE__
	#define I2C_ENABLED		1
#endif


#ifdef __cplusplus
extern "C" {
#endif 

typedef struct i2c_s_{
    char* name;
    uint16_t addr;
    int pointer;
    char exclusive;
} I2C;


// helper functions
int 	_i2c_getFd 				(I2C* dev, int *devHandle);
int 	_i2c_releaseFd			(int devHandle);

int 	_i2c_setDevice 			(int devHandle, int addr);
int 	_i2c_setDevice10bit 	(int devHandle, int addr);

int 	_i2c_writeBuffer		(I2C* dev, uint8_t *buffer, int size);
int 	_i2c_open_pointer 		(I2C* dev);
int     _i2c_close_pointer      (I2C* dev);


// i2c functions
I2C* 	i2c_init			    (int devNum, uint16_t devAddr);
int i2c_close_exclusive(I2C* dev);
int i2c_open_exclusive(I2C* dev);

int     _i2c_writeBuffer_fd     (I2C *dev,       uint8_t *buffer, int size);
int     _i2c_readRaw_fd         (I2C *dev, uint8_t *buffer, int numBytes);

int 	i2c_writeBuffer			(I2C* dev, uint8_t regAddr, uint8_t *buffer, int size);
int 	i2c_writeBufferRaw		(I2C* dev,                  uint8_t *buffer, int size);
int 	i2c_write	 			(I2C* dev, uint8_t regAddr, int val);
int 	i2c_writeBytes 			(I2C* dev, uint8_t regAddr, int val,         int numBytes);
int 	i2c_writeByte 			(I2C* dev, uint8_t regAddr, uint8_t val);
int 	i2c_read 				(I2C* dev, uint8_t regAddr, uint8_t *buffer, int numBytes);
int 	i2c_readRaw				(I2C* dev,                  uint8_t *buffer, int numBytes);
int 	i2c_readByte 			(I2C* dev, uint8_t regAddr, uint8_t *val);
void 	i2c_clear			    (I2C* dev);

#ifdef __cplusplus
}
#endif 
#endif // _ONION_I2C_H_ 
