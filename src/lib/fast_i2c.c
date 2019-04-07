#include <fast_i2c.h>

int _i2c_getFd(I2C* dev, int *devHandle)
{
    // create a file descriptor for the I2C bus
    *devHandle = open(dev->name, O_RDWR);
    S_ASSERT(*devHandle)
  	return EXIT_SUCCESS;
}
int _i2c_releaseFd(int devHandle){
    S_ASSERT(close(devHandle))
	return EXIT_SUCCESS;
}

int _i2c_setDevice(int devHandle, int addr){
	// set to 7-bit addr
    S_ASSERT(ioctl(devHandle, I2C_TENBIT, 0))
	// set the address
    S_ASSERT(ioctl(devHandle, I2C_SLAVE, addr))

	return EXIT_SUCCESS;
}
int _i2c_setDevice10bit(int devHandle, int addr)
{
	// set to 10-bit addr
    S_ASSERT( ioctl(devHandle, I2C_TENBIT, 1) )
	// set the address
    I_ASSERT( _i2c_setDevice(devHandle, addr) )
	return EXIT_SUCCESS;
}

I2C* i2c_init(int devNum, uint16_t devAddr){
    char pathname[255];
    int status = snprintf(pathname, sizeof(pathname), I2C_DEV_PATH, devNum);
    if(status < 0 && status >= sizeof(pathname)){
        return NULL;
    }
    I2C* ret = calloc(1, sizeof(I2C));
    ret->name = malloc(status + 1);
    strcpy(ret->name, pathname);
    ret->addr = devAddr;
    ret->pointer = -1;
    ret->exclusive = 0;
    return ret;
}
int i2c_open_exclusive(I2C* dev){
    PS_ASSERT(dev, _i2c_open_pointer(dev))
    dev->exclusive = 1;
    return EXIT_SUCCESS;
}
int i2c_close_exclusive(I2C* dev){
    dev->exclusive = 0;
    I_ASSERT(_i2c_close_pointer(dev))
    return EXIT_SUCCESS;
}

void i2c_clear(I2C* dev){
    if(dev->pointer >= 0) close(dev->pointer);
    free(dev->name);
    free(dev);
}
int _i2c_open_pointer(I2C* dev){
    if(dev->exclusive && dev->pointer >= 0) return EXIT_SUCCESS;
    if(dev->pointer < 0){
        S_ASSERT(dev->pointer = open(dev->name, O_RDWR))
    }
    PS_ASSERT(dev, ioctl(dev->pointer, I2C_TENBIT, dev->addr & 0x380))
    PS_ASSERT(dev, ioctl(dev->pointer, I2C_SLAVE, dev->addr & 0x3ff))
    return EXIT_SUCCESS;
}
int _i2c_close_pointer(I2C* dev){
    if(dev->pointer >= 0 && !dev->exclusive){
        S_ASSERT(close(dev->pointer))
        dev->pointer = -1;
    }
    return EXIT_SUCCESS;
}

int _i2c_writeBuffer_fd(I2C* dev, uint8_t *buffer, int size){
    ASSERT(write(dev->pointer, buffer, size) == size)
    return EXIT_SUCCESS;
}
int _i2c_readRaw_fd(I2C* dev, uint8_t *buffer, int numBytes){
    memset( buffer, 0, numBytes );
    ASSERT(read(dev->pointer, buffer, numBytes) == numBytes)
    return EXIT_SUCCESS;
}

int i2c_writeBufferRaw(I2C* dev, uint8_t *buffer, int size){
    PI_ASSERT(dev, _i2c_open_pointer(dev))
    PI_ASSERT(dev, _i2c_writeBuffer_fd(dev, buffer, size))
    I_ASSERT(_i2c_close_pointer(dev))
    return EXIT_SUCCESS;
}
int i2c_read(I2C* dev, uint8_t regAddr, uint8_t *buffer, int numBytes){
    PI_ASSERT(dev, _i2c_open_pointer(dev))
    PI_ASSERT(dev, _i2c_writeBuffer_fd(dev, &regAddr, 1))
    PI_ASSERT(dev, _i2c_readRaw_fd(dev, buffer, numBytes))
    I_ASSERT(_i2c_close_pointer(dev))
    return EXIT_SUCCESS;
}
int i2c_readRaw(I2C* dev, uint8_t *buffer, int numBytes){
    PI_ASSERT(dev, _i2c_open_pointer(dev))
    PI_ASSERT(dev, _i2c_readRaw_fd(dev, buffer, numBytes))
    I_ASSERT(_i2c_close_pointer(dev))
    return EXIT_SUCCESS;
}

int i2c_writeBuffer(I2C* dev, uint8_t regAddr, uint8_t *buffer, int size){
	int 	status;
	uint8_t *bufferNew;

	// allocate the new buffer
	size++;		// adding addr to buffer
	bufferNew 	= malloc( size * sizeof *bufferNew );

	// add the address to the data buffer
    bufferNew[0]	= regAddr;
	memcpy( &bufferNew[1], &buffer[0], size * sizeof *buffer );

 	// perform the write
    status 	= i2c_writeBufferRaw(dev, bufferNew, size);

 	// free the allocated memory
 	free(bufferNew);

	return (status);
}
int i2c_write(I2C* dev, uint8_t regAddr, int val){
	int 	size, tmp, index;
	uint8_t	buffer[I2C_BUFFER_SIZE]; 

	//// buffer setup
	// clear the buffer
	memset( buffer, 0, I2C_BUFFER_SIZE );
	// push the address and data values into the buffer
    buffer[0]	= regAddr;
	buffer[1]	= (val & 0xff);
	size 		= 2;

	// if value is more than 1-byte, add to the buffer
	tmp 	= (val >> 8);	// start with byte 1
	index	= 2;
	while (tmp > 0x00) {
		buffer[index] = (uint8_t)(tmp & 0xff);

		tmp	= tmp >> 8; // advance the tmp data by a byte
		index++; 		// increment the index

		size++;			// increase the size
    }

	// write the buffer
    return i2c_writeBufferRaw(dev, buffer, size);
}
int i2c_writeBytes(I2C* dev, uint8_t regAddr, int val, int numBytes){
	int 	size, index;
	uint8_t	buffer[I2C_BUFFER_SIZE];

	//// buffer setup
	// clear the buffer
	memset( buffer, 0, sizeof(buffer) );
	// push the address and data values into the buffer
    buffer[0]	= regAddr;
	size 		= 1;

	// add all data bytes to buffer
	index	= 1;
	for (index = 0; index < numBytes; index++) {
		buffer[index+1] = (uint8_t)( (val >> (8*index)) & 0xff );

		size++;			// increase the size
    }

	// write the buffer
    return i2c_writeBufferRaw(dev, buffer, size);
}
int i2c_writeByte(I2C* dev, uint8_t regAddr, uint8_t val){
    uint8_t	buffer[2];
    // push the address and data values into the buffer
    buffer[0]	= (regAddr);
    buffer[1]	= (val);

    return i2c_writeBufferRaw(dev, buffer, 2);
}
int i2c_readByte(I2C* dev, uint8_t regAddr, uint8_t *val){
    return i2c_read	(dev, regAddr, val, 1);
}

