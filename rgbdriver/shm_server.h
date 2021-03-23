/*
 * shm_server.h
 *
 *  Created on: 21 ��� 2018 �.
 *      Author: complynx
 */

#ifndef SRC_SHM_SERVER_H_
#define SRC_SHM_SERVER_H_

#include <stdio.h>
#include <unistd.h>
#include <float.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <pcadriver.h>

#define IPC_SHM_KEY     7867
#define UDP_SERVER_PORT 7867
#define TCP_SERVER_PORT 7868
#define UDP_PACKET_LENGTH_MAX 256
#define RECV_TIMEOUT_SEC 5
#define TCP_LISTEN_CONNECTIONS 10

#define QUERY_TYPE_DISCOVERY 0x01
#define QUERY_TYPE_REQUEST 0x02
#define QUERY_TYPE_RESPONSE 0x03
#define QUERY_TYPE_ERROR 0xfe

#define QUERY_ERROR_UNSPECIFIED 0x00
#define QUERY_ERROR_WRONG_TYPE 0x01
#define QUERY_ERROR_WRONG_REQUEST 0x02
#define QUERY_ERROR_BAD_REQUEST 0x03

#define DISCOVERY_PING 0x01
#define DISCOVERY_STATUS 0x02

#define RESPONSE_DISCOVERY_PING 0x01
#define RESPONSE_STATUS 0x02

#define DEVICE_TYPE_LED 0x01

#define CMD_SET_PROG 0x02
#define CMD_SET_MAIN_LIGHT 0x03
#define CMD_SET_COLOR 0x04
#define CMD_TOGGLE_MAIN_LIGHT 0x05

#define CRC32_START_MAGIC 0xCA7ADDED

typedef struct shm_struct_ {
    pid_t pid;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    double time;
    uint_fast8_t transition;
    uint_fast8_t exit_flag;
    uint8_t main_light;

    double position;
    duty_RGBW current_duty;
    uint8_t reset_start_color;
} shm_struct;


#endif /* SRC_SHM_SERVER_H_ */
