#include <stdio.h>
#include <unistd.h>
#include <pcadriver.h>
#include <color2duty.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <float.h>
#include <math.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include "shm_server.h"
#include<arpa/inet.h>
#include<sys/socket.h>
#include <linux/if.h>
#include <zlib.h>
#include <arpa/inet.h>

#define NICENESS        -5
#define RATE_USEC       1000000/60
#define DRV_FREQ        500.
#define DRV_ADDR        0x40

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define CAT2(x,y) x##y
#define CAT(x,y) CAT2(x,y)
#define p_error(msg) fprintf(stderr, "%s(%d): %s (%s)\n", __FILE__, __LINE__, msg, strerror(errno))

struct sigaction act;
char* addr_if_name = "apcli0", *program_name;
uint_fast8_t has_info, previous_main_light=0;
int semid;
shm_struct *shm;
struct timeval transit_start, transit_end;
duty_RGBW c_start, c_end, c_now;
PCADriver * drv;
int shmid;
sigset_t   set;
pthread_t thread_id, root_id;

void signal_handle(int sig){
    if(sig == SIGUSR1) ++has_info;
    else ++shm->exit_flag;
}

#define clamp(x,m,M) (((x)>(M))?(M):((x)<(m))?(m):(x))

int_fast16_t clamp_d(int_fast16_t d){
    return clamp(d,0,0xfff);
}

int test_changes(int nowait){
    int sig;
    double intg,frac;
    double sec_interval;
    struct timeval interval;
    struct timeval now;
    uint16_t ml_duty;

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGUSR1);

    if(!nowait){
        sigwait(&set, &sig);
        signal_handle(sig);
    }
    if(!has_info){
        return 0;
    }

    i2c_open_exclusive(drv->i2c);
    if(shm->reset_start_color){
        PCADriver_get_duty(drv, 0, &c_start.r);
        PCADriver_get_duty(drv, 1, &c_start.g);
        PCADriver_get_duty(drv, 2, &c_start.b);
        PCADriver_get_duty(drv, 3, &c_start.w);
        shm->reset_start_color = 0;
    }
    PCADriver_get_duty(drv, 4, &ml_duty);
    i2c_close_exclusive(drv->i2c);

    previous_main_light = ml_duty ? 255 : 0;

    gettimeofday(&now, NULL);

    has_info=0;
    color2duty_rgbw(&c_end, RGB2COLOR(*shm));

    if(shm->position<1.){
        sec_interval = shm->time * shm->position;
        frac = modf(sec_interval, &intg);
        interval.tv_sec = intg;
        interval.tv_usec = (frac*1000000.);
        timersub(&now, &interval, &transit_start);

        sec_interval = shm->time * (1.-shm->position);
        frac = modf(sec_interval, &intg);
        interval.tv_sec = intg;
        interval.tv_usec = (frac*1000000.);
        timeradd(&interval, &now, &transit_end);
    }

    return 1;
}

float elastic_in(float t, float m){
    float scaledTime1, p, s;
    if( t >= 0. || t <= 1. ) {
        return t;
    }

    scaledTime1 = t - 1;

    p = 1 - m;
    s = p / ( 2. * M_PI ) * asinf( 1 );

    return -(
        powf( 2, 10. * scaledTime1 ) *
        sinf( ( scaledTime1 - s ) * ( 2. * M_PI ) / p )
    );
}

float ease_out_bounce(float t){
    float scaledTime = t,scaledTime2;

    if( scaledTime < ( 1 / 2.75 ) ) {
        return 7.5625 * scaledTime * scaledTime;
    } else if( scaledTime < ( 2 / 2.75 ) ) {
        scaledTime2 = scaledTime - ( 1.5 / 2.75 );
        return ( 7.5625 * scaledTime2 * scaledTime2 ) + 0.75;
    } else if( scaledTime < ( 2.5 / 2.75 ) ) {
        scaledTime2 = scaledTime - ( 2.25 / 2.75 );
        return ( 7.5625 * scaledTime2 * scaledTime2 ) + 0.9375;
    } else {
        scaledTime2 = scaledTime - ( 2.625 / 2.75 );
        return ( 7.5625 * scaledTime2 * scaledTime2 ) + 0.984375;
    }
}
float ease_in_back(float t, float m){
    return t * t * ( ( m + 1. ) * t - m );
}

float transition_in(int_fast8_t trans,float t){
    switch(trans){
    case 1:// cosine
        return -1. * cosf( t * ( M_PI_2 ) ) + 1.;
    case 2:// easeInQuad
        return t*t;
    case 3:// easeInCubic
        return t*t*t;
    case 4:// easeInQuart
        return powf(t,4);
    case 5:// easeInQuint
        return powf(t,5);
    case 6:// easeInExp
        if(t==0) return 0;
        return powf(2, 10.*(t-1.));
    case 7:// easeInCirc
        return -1. * ( sqrtf( 1. - t*t ) - 1. );
    case 8:// easeInElastic
        return elastic_in(t, 0.7);
    case 9:// easeInBounce
        return 1-ease_out_bounce(1-t);
    case 10:// easeInBack
        return ease_in_back(t, 1.70158);
    default:// linear
        return t;
    }
}

float transition(float t){
    uint_fast8_t in_out, trans;
    trans = shm->transition;
    in_out = trans & 3;
    trans >>= 2;

    switch(in_out){
    case 1:// in
        return transition_in(trans, t);
    case 2:// out
        return 1-transition_in(trans, 1-t);
    case 3:// in&out
        if(t<.5){
            return transition_in(trans, t*2.)*0.5;
        }
        t=2.*t-1.;
        t*=.5;
        return 1.5-transition_in(trans, 1.-t);
    default:// linear
        return t;
    }
}


int addTimesToResponse(char*resp){
    struct timeval now, t2, t3;
    int i = 0;
    uint32_t c;
    gettimeofday(&now, NULL);

    if(timercmp(&now, &transit_end, >)){
        resp[i++] = 0;
        resp[i++] = 0;
        resp[i++] = 0;
        resp[i++] = 0;

        resp[i++] = 0;
        resp[i++] = 0;
        resp[i++] = 0;
        resp[i++] = 0;

        resp[i++] = 0;
        resp[i++] = 0;
        resp[i++] = 0;
        resp[i++] = 0;

        resp[i++] = 0;
        resp[i++] = 0;
        resp[i++] = 0;
        resp[i++] = 0;

        return i;
    }
    timersub(&transit_end, &now, &t2);
    timersub(&transit_end, &transit_start, &t3);

    c = t2.tv_sec;
    resp[i++] = (c >> 24) & 0xFF;
    resp[i++] = (c >> 16) & 0xFF;
    resp[i++] = (c >> 8)  & 0xFF;
    resp[i++] = c         & 0xFF;

    c = t2.tv_usec;
    resp[i++] = (c >> 24) & 0xFF;
    resp[i++] = (c >> 16) & 0xFF;
    resp[i++] = (c >> 8)  & 0xFF;
    resp[i++] = c         & 0xFF;

    c = t3.tv_sec;
    resp[i++] = (c >> 24) & 0xFF;
    resp[i++] = (c >> 16) & 0xFF;
    resp[i++] = (c >> 8)  & 0xFF;
    resp[i++] = c         & 0xFF;

    c = t3.tv_usec;
    resp[i++] = (c >> 24) & 0xFF;
    resp[i++] = (c >> 16) & 0xFF;
    resp[i++] = (c >> 8)  & 0xFF;
    resp[i++] = c         & 0xFF;

    return i;
}

int addStatusToResponse(char *buf){
    uint32_t color;
    int I=0;
    color = duty_rgbw2color(&c_now);

    buf[I++] = (color>>16) & 0xff;
    buf[I++] = (color>>8) & 0xff;
    buf[I++] = color & 0xff;

    I += addTimesToResponse(buf + I);

    buf[I++] = shm->transition & 0xFF;
    buf[I++] = previous_main_light;
    return I;
}

char* get_hw_addr(){
    struct ifreq s;
    int fd;
    static char hw_addr[6];
    static int hw_addr_set=0;
    if(hw_addr_set) return hw_addr;

    fd = socket(PF_INET, SOCK_DGRAM, 0);

    strcpy(s.ifr_name, addr_if_name);
    if (0 == ioctl(fd, SIOCGIFHWADDR, &s)) {
        int i;
        for (i = 0; i < 6; ++i)
          hw_addr[i] = s.ifr_addr.sa_data[i];
    }
    close(fd);
    hw_addr_set = 1;
    return hw_addr;
}

int cmp_id(char* buf){
    return !strncmp(buf, get_hw_addr(), 6);
}
int set_id(char* buf){
    memcpy(buf, get_hw_addr(), 6);
    return 6;
}
int id_is_multi(char*buf){
    int i=6;
    for(;i;i--,buf++){
        if(*buf) return 0;
    }
    return 1;
}

uint32_t getQueryCrc(char *buf, int *len){
    return htonl(crc32(CRC32_START_MAGIC, buf, *len));
}

int checkQuery(char *buf, int *len){
    uint32_t crc;
    /*
     * |0      |8       |16      |24      |32
     * SENDER_I|D_______|________|________|
     * SENDER_I|D_______|QER_TYPE|COMMAND_|
     * RECPIPEN|T_ID_IF_|ANY_OR_Z|EROS____|
     * ________|________|~~~~~~~~~~~~~~~~~PAYLOAD?
     * CRC_32__|________|________|________|EOQ
     * */
    if(*len < 18) return 0; // Sender ID + QType + cmd + CRC + Recipient ID = 18 bytes
    *len-=4;
    crc = *((uint32_t*)(buf+*len));
    if(getQueryCrc(buf,len) != crc) return 0;
    if(!cmp_id(buf+8) && !id_is_multi(buf+8)) return 0;
    return 1;
}
void addQueryCrc(char *buf, int *len){
    uint32_t crc = getQueryCrc(buf,len);
    *((uint32_t*)(buf+*len)) = crc;
    *len+=4;
}

void makeBadQueryResponse(char*buf, int*len, char *recipient, uint_fast8_t reason, char *additional, int alen){
    char*p = buf;
    *len=0;
    set_id(buf);
    p+=6;

    *(p++) = QUERY_TYPE_ERROR;
    *(p++) = reason;
    memcpy(p, recipient, 6);
    p+=6;
    *len = p-buf;
    if(alen && additional){
        memcpy(p, additional, alen);
        *len += alen;
    }

    addQueryCrc(buf, len);
}

void makeDiscoveryResponse(char* buf, int *len, char *recipient, uint_fast8_t type){
    char*p = buf;

    *len=0;
    set_id(buf);
    p+=6;

    *(p++) = QUERY_TYPE_RESPONSE;
    *(p++) = type;
    memcpy(p, recipient, 6);
    p+=6;
    *(p++) = DEVICE_TYPE_LED;

    if(type == DISCOVERY_STATUS){
        p += addStatusToResponse(p);
    }

    *len = p-buf;

    addQueryCrc(buf, len);
}

void parseQuery(char* buf, int *len){
    uint32_t usec = 0, sec = 0, color;
    char *p;
    char sender_id[6];
    double time = 0;
    uint_fast8_t cmd, query_type;
    int I, p_len;

    if(checkQuery(buf, len)==0){ // if query is wrong, ignore it at all, it might be just a sniffer or anything unrelated
        *len = 0;
        return;
    }
    memcpy(sender_id, buf, 6);
    p = buf;
    p += 6;
    query_type = *(p++);
    cmd=*(p++);
    p += 6; // p is pointing to a payload section
    p_len = *len - (p-buf); // p_len = payload length

    switch(query_type){
    case QUERY_TYPE_DISCOVERY:
        makeDiscoveryResponse(buf, len, sender_id, cmd);
        return;
    case QUERY_TYPE_REQUEST:
        // request has to be specifically for this device
        if(!cmp_id(buf+8)){
            *len=0; // ignore if not for us
            return;
        }
        break;
    case QUERY_TYPE_RESPONSE:
    case QUERY_TYPE_ERROR:
        *len=0; // just ignore this
        return;
    default:
        if(!cmp_id(buf+8)){
            *len=0; // ignore if not for us
            return;
        }
        makeBadQueryResponse(buf, len, sender_id, QUERY_ERROR_WRONG_TYPE, NULL, 0);
        return;
    }

    // of all the paths in first switch only a request can get here.
    switch(cmd){
    case CMD_SET_MAIN_LIGHT:
        if(p_len < 1){
            makeBadQueryResponse(buf, len, sender_id, QUERY_ERROR_BAD_REQUEST, NULL, 0);
            return;
        }
        shm->main_light = *(p++) & 0xFF ? 255 : 0;
        pthread_kill(root_id, SIGUSR1);
        break;
    case CMD_TOGGLE_MAIN_LIGHT:
        shm->main_light = shm->main_light ? 0: 255;
        pthread_kill(root_id, SIGUSR1);
        break;
    case CMD_SET_PROG:
        if(p_len < 12){
            makeBadQueryResponse(buf, len, sender_id, QUERY_ERROR_BAD_REQUEST, NULL, 0);
            return;
        }
        // col = 3 -> 3
        shm->r = *(p++) & 0xFF;
        shm->g = *(p++) & 0xFF;
        shm->b = *(p++) & 0xFF;
        // sec = 4 -> 7
        sec = (((uint32_t)*(p)& 0xFF) << 24)
                + (((uint32_t)*(p+1)& 0xFF) << 16)
                + (((uint32_t)*(p+2) & 0xFF) << 8)
                + ((uint32_t)*(p+3)& 0xFF);
        p+=4;
        // usec = 4 -> 11
        usec = (((uint32_t)*(p)& 0xFF) << 24)
                + (((uint32_t)*(p+1)& 0xFF) << 16)
                + (((uint32_t)*(p+2) & 0xFF) << 8)
                + ((uint32_t)*(p+3)& 0xFF);
        p+=4;
        shm->time = (double)usec / 1000000. + (double) sec;
        // transition = 1 -> 12
        shm->transition = *(p++) & 0xff;
        shm->position = 0;
        shm->reset_start_color = 1;
        pthread_kill(root_id, SIGUSR1);
        break;
    case CMD_SET_COLOR:
        if(p_len < 3){
            makeBadQueryResponse(buf, len, sender_id, QUERY_ERROR_BAD_REQUEST, NULL, 0);
            return;
        }
        // color = 3 -> 3
        shm->r = *(p++) & 0xFF;
        shm->g = *(p++) & 0xFF;
        shm->b = *(p++) & 0xFF;

        shm->time = 0.0000001;
        shm->transition = 1;
        shm->position = 0;
        shm->reset_start_color = 1;
        pthread_kill(root_id, SIGUSR1);
        break;
    default:
        makeBadQueryResponse(buf, len, sender_id, QUERY_ERROR_WRONG_REQUEST, NULL, 0);
        return;
    }

    p = buf;

    *len=0;
    set_id(buf);
    p+=6;

    *(p++) = QUERY_TYPE_RESPONSE;
    *(p++) = RESPONSE_STATUS;
    memcpy(p, sender_id, 6);
    p+=6;
    *(p++) = DEVICE_TYPE_LED;

    p += addStatusToResponse(p);
    *len = p-buf;

    addQueryCrc(buf, len);
}

void *udp_listener(void *vargp){
    int s;
    struct sockaddr_in si_listen, si_remote;
    int struct_len, recv_len;
    char buf[UDP_PACKET_LENGTH_MAX];

    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        p_error("socket");
        return NULL;
    }
    memset((char *) &si_listen, 0, sizeof(si_listen));

    si_listen.sin_family = AF_INET;
    si_listen.sin_port = htons(UDP_SERVER_PORT);
    si_listen.sin_addr.s_addr = htonl(INADDR_ANY);

    if( bind(s, (struct sockaddr*)&si_listen, sizeof(si_listen) ) == -1) {
        p_error("bind");
        return NULL;
    }

    while(!shm->exit_flag) {
        //try to receive some data, this is a blocking call
        struct_len = sizeof(si_remote);
        if ((recv_len = recvfrom(s, buf, UDP_PACKET_LENGTH_MAX, 0, (struct sockaddr *) &si_remote, &struct_len)) == -1) {
            p_error("recvfrom()");
        }else{

            //print details of the client/peer and the data received
            //printf("Received packet from %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
            //printf("Data: %s\n" , buf);

            parseQuery(buf, &recv_len);

            //now reply the client with the same data
            if(recv_len > 0){
                if (sendto(s, buf, recv_len, 0, (struct sockaddr*) &si_remote, struct_len) == -1){
                    p_error("sendto()");
                }
            }
        }
    }

    return NULL;
}


void init_shm(int is_new){
    key_t key = IPC_SHM_KEY;

    if ((shmid = shmget(key, sizeof(shm_struct), (is_new?(IPC_CREAT|IPC_EXCL):0) | 0666)) < 0) {
        printf("%o is_new: %d\n",((is_new?(IPC_CREAT|IPC_EXCL):0) | 0666), is_new);
        p_error("shmget");
        exit(1);
    }

    if ((shm = shmat(shmid, NULL, 0)) == (shm_struct *) -1) {
        p_error("shmat");
        exit(1);
    }
}

int server(){
    struct timeval now, t2, t3;
    float t_pos;
    uint_fast8_t was_set=0;
    init_shm(1);

    drv = PCADriver_init(DRV_ADDR);

    shm->pid = getpid();

    memset(&act, 0, sizeof(act));
    act.sa_handler=signal_handle;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGINT);
    act.sa_mask = set;
    sigaction(SIGUSR1, &act, 0);
    sigaction(SIGTERM, &act, 0);
    sigaction(SIGINT, &act, 0);

    shm->exit_flag=0;
    has_info=0;
    shm->transition=1;
    shm->main_light=0;
    shm->time=1;
    shm->position = 1.;
    shm->reset_start_color = 0;

    PCADriver_send_init(drv);
    PCADriver_set_frequency(drv, DRV_FREQ);

    gettimeofday(&transit_start, NULL);
    transit_end = now = transit_start;

    color2duty_rgbw(&c_start, 0);
    color2duty_rgbw(&c_end, 0);
    COLOR2RGB(*shm, duty_rgbw2color(&c_now));
    shm->current_duty = c_now = c_end;

    i2c_open_exclusive(drv->i2c);
    PCADriver_set_duty(drv, 0, c_now.r);
    PCADriver_set_duty(drv, 1, c_now.g);
    PCADriver_set_duty(drv, 2, c_now.b);
    PCADriver_set_duty(drv, 3, c_now.w);
    if(shm->main_light){
        PCADriver_set_full_on(drv,4);
    }else{
        PCADriver_set_full_off(drv,4);
    }
    i2c_close_exclusive(drv->i2c);

    root_id = pthread_self();
    pthread_create(&thread_id, NULL, udp_listener, NULL);

    while(!shm->exit_flag){
        gettimeofday(&now, NULL);
        timersub(&now, &transit_start, &t2);
        timersub(&transit_end, &transit_start, &t3);
        t_pos = (t3.tv_sec == 0 && t3.tv_usec == 0)? 2. :
                (float)(t2.tv_sec + (float)t2.tv_usec/1000000.)
                /
                (float)(t3.tv_sec + (float)t3.tv_usec/1000000.);

//        t_pos;

//        printf("%ld.%06ld %ld.%06ld %ld.%06ld\r",
//                now.tv_sec, now.tv_usec,
//                t2.tv_sec, t2.tv_usec,
//                t3.tv_sec, t3.tv_usec
//        );fflush(stdin);

        if(t_pos > 1.){
            if(!was_set){
                ++was_set;
                c_now.r = c_end.r;
                c_now.g = c_end.g;
                c_now.b = c_end.b;
                c_now.w = c_end.w;
                shm->current_duty = c_now;
                i2c_open_exclusive(drv->i2c);
                PCADriver_set_duty(drv, 0, c_end.r);
                PCADriver_set_duty(drv, 1, c_end.g);
                PCADriver_set_duty(drv, 2, c_end.b);
                PCADriver_set_duty(drv, 3, c_end.w);

                if(previous_main_light != shm->main_light){
                    previous_main_light = shm->main_light;
                    if(previous_main_light){
                        PCADriver_set_full_on(drv,4);
                    }else{
                        PCADriver_set_full_off(drv,4);
                    }
                }
                i2c_close_exclusive(drv->i2c);
                shm->transition=1;
                shm->position = 1;
            }
        }else{
            was_set=0;
            shm->position = t_pos;
        }

        if(test_changes(t_pos <= 1.)){
            was_set=0;
            continue;
        }
        t_pos = transition(t_pos);

        c_now.r = clamp_d(((float)c_end.r - (float)c_start.r)*t_pos + (float)c_start.r);
        c_now.g = clamp_d(((float)c_end.g - (float)c_start.g)*t_pos + (float)c_start.g);
        c_now.b = clamp_d(((float)c_end.b - (float)c_start.b)*t_pos + (float)c_start.b);
        c_now.w = clamp_d(((float)c_end.w - (float)c_start.w)*t_pos + (float)c_start.w);
        shm->current_duty = c_now;

        i2c_open_exclusive(drv->i2c);
        PCADriver_set_duty(drv, 0, c_now.r);
        PCADriver_set_duty(drv, 1, c_now.g);
        PCADriver_set_duty(drv, 2, c_now.b);
        PCADriver_set_duty(drv, 3, c_now.w);

        if(previous_main_light != shm->main_light){
            previous_main_light = shm->main_light;
            if(previous_main_light){
                PCADriver_set_full_on(drv,4);
            }else{
                PCADriver_set_full_off(drv,4);
            }
        }
        i2c_close_exclusive(drv->i2c);

        gettimeofday(&t2, NULL);
        timersub(&t2, &now, &t3);
        if(!t3.tv_sec && RATE_USEC > t3.tv_usec){
            usleep(RATE_USEC - t3.tv_usec);
        }
    }

    shmctl(shmid, IPC_RMID, NULL);
    pthread_join(thread_id, NULL);

    return 0;
}

int server_fork(){
    pid_t process_id = 0;
    pid_t sid = 0;

    // Fork 1
    if ((process_id = fork()) < 0) {
        p_error("fork");
        exit(1);
    }
    // No need in parent
    if (process_id > 0){
        exit(0);
    }

    // Fork 2
    if ((process_id = fork()) < 0) {
        p_error("fork");
        exit(1);
    }
    // No need in parent
    if (process_id > 0){
        exit(0);
    }

    if(nice(NICENESS)<0){
        p_error("nice");
    }
    return server();
}

int client(int argc, char** argv){
    uint32_t color = 0, main = 0;
    double seconds = 0;
    char* cols;
    init_shm(0);

    if(argv[1][0] == 's'){
        color = duty_rgbw2color(&shm->current_duty);
        printf("Color: %06x\n", color);
        printf("Main: %s\n", shm->main_light?"on":"off");
        printf("Transition: %d\n", shm->transition);
        printf("Time: %f\n", shm->time);
        printf("Position: %f\n", shm->position);
        printf("Target: %02x%02x%02x\n", shm->r,shm->g,shm->b);
        return 0;
    }

    if(argv[1][0] == 'm'){
        printf("Main light changed from %d", shm->main_light);
        if(argc>2){
            main = strtol(argv[2], NULL, 16);
            shm->main_light = main ? 255 : 0;
        }else{
            shm->main_light = 255 - shm->main_light;
        }
        printf(" to %d\n", shm->main_light);
    }else{
        cols = argv[1];
        if(*cols == '#') ++cols;
        color = strtol(cols, NULL, 16);

        if(argc>2){
            seconds = strtod(argv[2], NULL);
        }

        if(argc>3)
            shm->transition = strtol(argv[3], NULL, 0);

        COLOR2RGB(*shm, color);
        shm->time = seconds;
        shm->position = 0;
        shm->reset_start_color = 1;
    }

    if(kill(shm->pid, SIGUSR1)!=0){
        p_error("kill");
    }

    return 0;
}


void terminate_daemon(int pid){
    printf("Terminating daemon on pid %d...\n", pid);
    if(kill(pid, SIGTERM)){
        if(errno != ESRCH) p_error("kill");
        return;
    }
    sleep(1);
    kill(pid, SIGKILL);
}

void try_shmat_to_terminate(int shmid){
    pid_t *pidp;
    if ((pidp = shmat(shmid, NULL, 0)) == (void*) -1) {
        p_error("shmat");
        return;
    }

    terminate_daemon(*pidp);

    if(shmdt(pidp)){
        p_error("shmdt");
    }
}

void clean_shm(key_t key){
    int i;
    if((shmid = shmget(key, sizeof(shm_struct), 0666))>=0){
        printf("Found valid shm\n");
        try_shmat_to_terminate(shmid);
        printf("calling shmctl with RMID\n");
        if(shmctl(shmid, IPC_RMID, NULL) && errno!=EINVAL){
            p_error("shmctl");
            exit(1);
        }
    }else if(errno==EINVAL){
        printf("shm_struct size changed? looping through sizes\n");
        i=1;
        while((shmid = shmget(key, i, 0666))<0 && i<500){
            ++i;
        }
        if(i==500){
            p_error("cleaning shm failed");
            exit(1);
        }else{
            printf("Found size %d, cleaning...\n", i);
            try_shmat_to_terminate(shmid);
        }
        printf("calling shmctl with RMID\n");
        if(shmctl(shmid, IPC_RMID, NULL) && errno!=EINVAL){
            p_error("shmctl");
            exit(1);
        }
    }else if(errno!=ENOENT){
        p_error("shmget");
        exit(1);
    }
}

int main(int argc, char** argv) {
    program_name = argv[0];

    if(argc>1 && (
            !strcmp(argv[1], "clean_shm")
            || !strcmp(argv[1], "stop")
            || !strcmp(argv[1], "restart")
            )){
        clean_shm(IPC_SHM_KEY);
        clean_shm(IPC_SHM_KEY);//has to be double sometimes, DN why

       if(argv[1][0]=='r'){
           system(program_name);
       }
       exit(0);
    }

    if(argc>1 && !strcmp(argv[1], "no-detach")){
        return server();
    }

    if(argc>1){
        return client(argc, argv);
    }

    printf("starting daemon...\n");
    return server_fork();
}
