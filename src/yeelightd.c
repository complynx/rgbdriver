#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <ifaddrs.h>
#include <json-c/json.h>
#include <linux/if.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <zlib.h>

#define NICENESS 0
#define UDP_SERVER_PORT 1982
#define MCAST_GROUP "239.255.255.250"
#define SERVER_STRING "CustomLight Complynx/1.0"
#define MODEL_STRING "ceiling"
#define FW_VER 18
#define UDP_PACKET_LENGTH_MAX 1024
#define TCP_PACKET_LENGTH_MAX 1024
#define RECV_TIMEOUT_SEC 5
#define POLL_TIMEOUT_INC 0.1
#define POLL_TIMEOUT_MIN 0.001
#define TCP_LISTEN_CONNECTIONS 10
#define TCP_FDS_MAX 200
#define ADV_SEND_TIME 3600
#define ADDRESS_INVALIDATION 10
#define ADV_SEND_TIME_TICK ((ADV_SEND_TIME)/(RECV_TIMEOUT_SEC))
#define RGBDRIVER_CMD "rgbdriver"

#define lprint(msg) fprintf(stderr, "%s(%d): %s\n", __FILE__, __LINE__, msg)
#define p_error(msg) fprintf(stderr, "%s(%d): %s (%s)\n", __FILE__, __LINE__, msg, strerror(errno))
#define p_errorce(msg,sock) fprintf(stderr, "%s(%d): %s (%s)\n", __FILE__, __LINE__, msg, strerror(errno));close(sock);exit(-1)
#define p_errorc(msg,sock) fprintf(stderr, "%s(%d): %s (%s)\n", __FILE__, __LINE__, msg, strerror(errno));close(sock)

int detached = 0;
struct sigaction act;
char* addr_if_name = "apcli0", *program_name, host_value[255];
pthread_t udp_listener_id, udp_sender_id, root_id, tcp_server_id,
	driver_commander_id;
int exit_flag = 0, tcp_port=0;
int ssdp_socket = 0, tcp_socket=0;
pthread_cond_t  got_tcp_port = PTHREAD_COND_INITIALIZER;
pthread_mutex_t tcp_port_wait_locker = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t  has_tasks = PTHREAD_COND_INITIALIZER;
pthread_mutex_t task_wait_mux = PTHREAD_MUTEX_INITIALIZER;

struct state {
	uint32_t color;
	int main_state;
	int transition;
	double time;
	double position;
	uint32_t target;
};
struct task {
	uint32_t color;
	int transition;
	double time;
	int main_state;
};
struct state current_state;
struct task current_task={
		.color=0,
		.transition=-1,
		.time=-1,
		.main_state=-1
};
pthread_mutex_t task_manipulation = PTHREAD_MUTEX_INITIALIZER;

typedef struct json_object* (*control_method_func)(struct json_object* params);
struct method {
	char* name;
	control_method_func func;
};
const struct method methods[];

const char *const control_errors[] = {
	"Unknown error",
	"No method",
	"Unknown method"
};


struct client_holder {
	struct client_holder*left;
	struct client_holder*right;

	int socket;
	struct sockaddr_in address;
	pthread_t thread;
};
pthread_mutexattr_t client_holder_manipulation_attr;
pthread_mutex_t client_holder_manipulation;
struct client_holder* client_holder_start = NULL;
struct client_holder* client_holder_end = NULL;
void client_holder_push(struct client_holder*new) {
	if(new==NULL) return;
	pthread_mutex_lock(&client_holder_manipulation);
	new->left = client_holder_end;
	client_holder_end = new;
	if(new->left != NULL) {
		new->left->right = new;
	}
	if(client_holder_start==NULL){
		client_holder_start = new;
	}
	pthread_mutex_unlock(&client_holder_manipulation);
}
void client_holder_remove(struct client_holder*old) {
	if(old==NULL) return;
	pthread_mutex_lock(&client_holder_manipulation);
	if(old->left != NULL) {
		old->left->right = old->right;
	}else{
		client_holder_start = old->right;
	}
	if(old->right != NULL) {
		old->right->left = old->left;
	}else{
		client_holder_end = old->left;
	}
	pthread_mutex_unlock(&client_holder_manipulation);
}


void signal_handle(int sig){
	++exit_flag;
	if(exit_flag>3){
		if(!detached) printf("forcing exit");
		exit(-1);
	}
}

int call_rgbdriver(const char *arg){
	pid_t pid = 0;
	int pipefd[2];
	FILE* output;
	int status, len=0;
	char line[256];
	struct json_object* obj, *param;
	enum json_tokener_error jerr;
	const char* str;
	struct json_tokener *tok=json_tokener_new();

	if(pipe(pipefd)<0) return -1;
	if((pid = fork())<0) return -1;
	if (pid == 0) {
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		dup2(pipefd[1], STDERR_FILENO);
		execlp(RGBDRIVER_CMD, RGBDRIVER_CMD, "j", arg, (char*) NULL);
	}

	close(pipefd[1]);

	do{
		len=read(pipefd[0], line, sizeof(line));
		if(len<=0) break;
		if(len<sizeof(line)){
			line[len]=0;
		}

		obj = json_tokener_parse_ex(tok, line, len);
	}while((jerr = json_tokener_get_error(tok))==json_tokener_continue);

	if((jerr = json_tokener_get_error(tok)) == json_tokener_success){
		if(json_object_is_type(obj, json_type_object)) {
			if(json_object_object_get_ex(obj, "color", &param) &&
					param != NULL) {
				current_state.color=json_object_get_int(param);
			}
			if(json_object_object_get_ex(obj, "main", &param) &&
					param != NULL) {
				current_state.main_state=json_object_get_int(param);
			}
			if(json_object_object_get_ex(obj, "transition", &param) &&
					param != NULL) {
				current_state.transition=json_object_get_int(param);
			}
			if(json_object_object_get_ex(obj, "time", &param) &&
					param != NULL) {
				current_state.time=json_object_get_double(param);
			}
			if(json_object_object_get_ex(obj, "position", &param) &&
					param != NULL) {
				current_state.position=json_object_get_double(param);
			}
			if(json_object_object_get_ex(obj, "target", &param) &&
					param != NULL) {
				current_state.target=json_object_get_int(param);
			}
		}
	}

	json_tokener_free(tok);

	waitpid(pid, &status, 0);
	return status;
}

void block_thread_signals(){
    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGUSR1);

    pthread_sigmask(SIG_BLOCK, &set, NULL);
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

char* addstr(char*buf, char*src){
	int len = strlen(src);
	memcpy(buf, src, len);
	return buf + len;
}

char* addstrln(char*buf, char*src){
	buf = addstr(buf, src);
	return addstr(buf, "\r\n");
}

char* add_udp_answer_prefix(char*buf) {
	return addstrln(buf, "HTTP/1.1 200 OK");
}

char* add_udp_adv_prefix(char*buf) {
	return addstrln(buf, "NOTIFY * HTTP/1.1");
}

char* add_host(char*buf) {
	buf = addstr(buf, "Host: ");
	return addstrln(buf, host_value);
}

char* add_cache_control(char*buf) {
	buf=addstr(buf, "Cache-Control: max-age=");
	return buf+sprintf(buf, "%d\r\n", ADV_SEND_TIME);
}

char* add_date(char*buf) {
	time_t rawtime;
	struct tm *info;
	char buffer[80];

	time( &rawtime );
	info = localtime( &rawtime );

	return buf+strftime(buf,80,"Date: %a, %d %b %Y %X %Z\r\n", info);
}

char* add_model(char*buf){
	return buf+sprintf(buf, "model: %s\r\n", MODEL_STRING);
}

char* add_fw(char*buf){
	return buf+sprintf(buf, "FW_VER: %d\r\n", FW_VER);
}

char* add_stats(char*buf){
	return buf+sprintf(buf, "power: on\r\n"
			"name: omega\r\n"
			"bright: %d\r\n"
			"ct: 2700\r\n"
			"rgb: 16777215\r\n"
			"hue: 0\r\n"
			"sat: 0\r\n"
			"color_mode: 2\r\n"
			"bg_power: on\r\n"
			"bg_lmode: 1\r\n"
			"rgb: %d\r\n",
			current_state.main_state*100/255,
			current_state.color);
}

char* add_server(char*buf){
	return buf+sprintf(buf, "Server: %s\r\n", SERVER_STRING);
}

char* add_id(char*buf){
	int i;
	char*addr=get_hw_addr();
	buf += sprintf(buf, "id: 0xc160");

	for(i=0;i<6;++i){
		buf += sprintf(buf, "%02x",addr[i] & 0xff);
	}
	return buf+sprintf(buf, "\r\n");
}

char* get_address_by_name(char *if_name) {
	struct ifaddrs *ifa, *tmp;
	getifaddrs(&ifa);
	tmp = ifa;
	char*ret = NULL;

	while (tmp){
	    if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET &&
	    		(if_name==NULL || strcmp(if_name, tmp->ifa_name)==0))
	    {
	        struct sockaddr_in *pAddr = (struct sockaddr_in *)tmp->ifa_addr;
	        ret = inet_ntoa(pAddr->sin_addr);
	        break;
	    }

	    tmp = tmp->ifa_next;
	}

	freeifaddrs(ifa);
	return ret;
}

char* get_address(){
	time_t cur=time(NULL);
	static char address[255]="";
	static time_t last=0;
	char*addr_tmp;

	if(last == 0 || difftime(last, cur)>ADDRESS_INVALIDATION) {
		addr_tmp = get_address_by_name(addr_if_name);
		if(addr_tmp == NULL) {
			addr_tmp = get_address_by_name(NULL);
		}
		if(addr_tmp != NULL) {
			strcpy(address, addr_tmp);
		}
		last = cur;
	}
	return address;
}

char *add_location(char*buf){
	return buf + sprintf(buf, "Location: yeelight://%s:%d\r\n",
			get_address(), tcp_port);
}

char *add_support(char*buf) {
	const struct method* I;
	buf = buf+sprintf(buf, "support:");
	for(I=methods;I->name;++I){
		buf = buf+sprintf(buf, " %s", I->name);
	}
	return buf;
}

char* add_udp_state(char*buf) {
	buf = add_cache_control(buf);
	buf = add_model(buf);
	buf = add_fw(buf);
	buf = add_server(buf);
	buf = add_id(buf);
	buf = add_location(buf);
	buf = add_support(buf);
	buf = add_stats(buf);
	return addstr(buf, "\r\n");
}

void *udp_sender(void *vargp) {
	int ticks = ADV_SEND_TIME_TICK;
	char message[UDP_PACKET_LENGTH_MAX], *s;

    struct sockaddr_in addr;

    block_thread_signals();

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(MCAST_GROUP);
	addr.sin_port = htons(UDP_SERVER_PORT);

	while(!exit_flag) {
		//try to receive some data, this is a blocking call
		if(ticks >= ADV_SEND_TIME_TICK) {
			if(!detached) printf("sending adv\n");

			s=message;
			s=add_udp_adv_prefix(s);
			s=add_host(s);
			s=addstrln(s, "NTS: ssdp:alive");
			s=add_udp_state(s);

			if(sendto(ssdp_socket, message, s-message, 0,
					(struct sockaddr*) &addr, sizeof(addr))<0){
				p_error("sendto adv");
			}

			ticks = 0;
		}
		sleep(RECV_TIMEOUT_SEC);
		++ticks;
	}
	if(!detached) printf("udp_sender finished\n");

	return NULL;
}

int getNextHeader(char*buf, int len, char**headerName, char**headerVal){
	char *i, *j;
	if(len>1 && *buf=='\r' && *(buf+1)=='\n'){
		return 0;
	}
	*headerName = i = buf;
	for(;i<buf+len;++i){
		if(isspace(*i)){
			return -1;
		}
		if(*i==':') {
			*i='\0';
			++i;
			break;
		}
		*i=tolower(*i);
	}
	for(;i<buf+len;++i){
		if(*i!= ' ' && *i != '\t'){
			break;
		}
	}
	*headerVal = i;
	for(;i<buf+len;++i){
		if(*i=='\n' && *(i-1)=='\r'){
			for(j=i;j>=*headerVal;--j) {
				if(!isspace(*j)){
					*(j+1) = '\0';
					break;
				}
			}
			return i+1 - buf;
		}
	}
	return -2;
}

int validate_msearch(int len, char *buf) {
	char *s=buf, *c,*end=buf+len, *headerName, *headerVal;
	int parsedLen=0, st_is_bulb=0, man_is_discover=0;
	const char* const REQ = "M-SEARCH * HTTP/1.1\r\n";

	buf[len]='\0';

	if(len < strlen(REQ)) {
		return 0;
	}
	if(memcmp(s, REQ, strlen(REQ)) != 0){
		return 0;
	}
	s += strlen(REQ);

	for(c=s;c<end;c+=parsedLen){
		parsedLen = getNextHeader(c, end-c, &headerName, &headerVal);
		if(parsedLen<0) {
			return 0;
		}
		if(parsedLen == 0) {
			break;
		}
		if(strcmp(headerName, "man")==0){
			if(strcmp(headerVal, "\"ssdp:discover\"")==0) {
				man_is_discover = 1;
			}else{
				return 0;
			}
		}else if(strcmp(headerName, "st")==0){
			if(strcmp(headerVal, "wifi_bulb")==0) {
				st_is_bulb = 1;
			}else{
				return 0;
			}
		}else if(strcmp(headerName, "host")==0){
			if(strcmp(headerVal, host_value)!=0) {
				return 0;
			}
		}
	}

	return 1;
}

void *udp_listener(void *vargp){
    int i;
    struct sockaddr_in si_listen, si_remote;
    int struct_len, recv_len;
    char buf[UDP_PACKET_LENGTH_MAX], *s;
    struct timeval tv;

    block_thread_signals();

    if ((ssdp_socket=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        p_error("socket");
        return NULL;
    }
    memset((char *) &si_listen, 0, sizeof(si_listen));

    si_listen.sin_family = AF_INET;
    si_listen.sin_port = htons(UDP_SERVER_PORT);
    si_listen.sin_addr.s_addr = htonl(INADDR_ANY);

	tv.tv_sec = RECV_TIMEOUT_SEC;
	tv.tv_usec = 0;
	setsockopt(ssdp_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);


    if( bind(ssdp_socket, (struct sockaddr*)&si_listen, sizeof(si_listen) ) == -1) {
        p_error("bind");
        return NULL;
    }

    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(MCAST_GROUP);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if ( setsockopt(ssdp_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &mreq,
    		sizeof(mreq)) < 0){
        p_error("setsockopt IP_ADD_MEMBERSHIP");
        return NULL;
    }
    if(!detached){
    	printf("UDP Listener created\n");
    }

    pthread_create(&udp_sender_id, NULL, udp_sender, NULL);

    while(!exit_flag) {
        //try to receive some data, this is a blocking call
        struct_len = sizeof(si_remote);
        if ((recv_len = recvfrom(ssdp_socket, buf, UDP_PACKET_LENGTH_MAX, 0,
        		(struct sockaddr *) &si_remote, &struct_len)) == -1) {
        	if (errno != EAGAIN && errno != EWOULDBLOCK) {
        		p_error("recvfrom()");
        	}
        }else{

            //print details of the client/peer and the data received
            if(!detached){
                printf("Received packet from %s:%d of len %d\n",
                		inet_ntoa(si_remote.sin_addr),
						ntohs(si_remote.sin_port), recv_len);

            }
            if(validate_msearch(recv_len, buf)){
            	if(!detached)printf("sending answer\n");

    			s=buf;
    			s=add_udp_answer_prefix(s);
    			s=add_date(s);
    			s=addstrln(s, "Ext: ");
    			s=add_udp_state(s);
    			if (sendto(ssdp_socket, buf, s-buf, MSG_CONFIRM,
    					(const struct sockaddr*) &si_remote, struct_len) == -1){
				    p_error("sendto repl");
    			}
            }
        }
    }
    if(!detached) printf("udp_listener finished\n");

    pthread_join(udp_sender_id, NULL);

    close(ssdp_socket);

    return NULL;
}

void client_destroy(struct client_holder *client){
	pthread_mutex_lock(&client_holder_manipulation);
	if(client->socket>=0){
		shutdown(client->socket, SHUT_RDWR);
		close(client->socket);
	}
    client_holder_remove(client);
    free(client);
	pthread_mutex_unlock(&client_holder_manipulation);
}

struct json_object* create_error_desc(int err_no,
		const char* const desc){
	struct json_object* err = json_object_new_object(), *err_obj;
	err_obj = json_object_new_object();
	json_object_object_add(err_obj, "code", json_object_new_int(err_no));
	json_object_object_add(err_obj, "message", json_object_new_string(desc));
	json_object_object_add(err, "error", err_obj);
	return err;
}

struct json_object* create_ok_result(){
	struct json_object*res_obj;
	res_obj = json_object_new_array();
	json_object_array_add(res_obj, json_object_new_string("ok"));
	return res_obj;
}

struct json_object* create_error(int err){
	return create_error_desc(err, control_errors[err]);
}

struct json_object* wrap_results(struct json_object* res_obj){
	struct json_object* res = json_object_new_object();
	json_object_object_add(res, "result", res_obj);
	return res;
}
#define CR_OK (wrap_results(create_ok_result()))

struct json_object* test_method(struct json_object* params) {
	if(!detached) printf("test_method called with %s\n",
			json_object_get_string(params));
	return CR_OK;
}
const struct method methods[] = {
		{
				"test",
				test_method
		},
		{}
};

struct json_object* process_control_object(struct json_object* obj){
	struct json_object *id_obj, *method_obj, *params_obj, *ret;
	const struct method* I;
	int id;
	const char *method_name;
	if(json_object_is_type(obj, json_type_object)){
		if(!json_object_object_get_ex(obj, "id", &id_obj)){
			if(!detached) printf("no id\n");
			return NULL; // skip the message
		}
		if(!detached) printf("id: %s\n", json_object_get_string(id_obj));
		if(!json_object_object_get_ex(obj, "method", &method_obj)) {
			if(!detached) printf("no method\n");
			ret=create_error(1);
			json_object_object_add(ret, "id", id_obj);
			return ret;
		}
		method_name = json_object_get_string(method_obj);
		if(!detached) printf("method: %s\n", method_name);
		json_object_object_get_ex(obj, "params", &params_obj);
		if(!detached) printf("params: %s\n", json_object_get_string(params_obj));
		for(I=methods;I->name;++I){
			if(!strcmp(method_name, I->name)) {
				break;
			}
		}
		if(!I->name) {
			ret=create_error(2);
		}else{
			ret = I->func(params_obj);
		}
		json_object_object_add(ret, "id", id_obj);
		return ret;

	}else{
		if(!detached) printf("not an object\n");
	}
	return NULL;
}

int process_control_request(int answer_to, char*buf, int len) {
	struct json_object* obj, *ret;
	struct json_tokener *tok=json_tokener_new();
	const char *jsonstr;
	char *msg;
	int ans_len, errno_old=errno;
	obj = json_tokener_parse_ex(tok, buf, len);
	if(json_tokener_get_error(tok) == json_tokener_success && obj != NULL){
		ret=process_control_object(obj);
		if(ret != NULL) {
			jsonstr=json_object_to_json_string_ext(ret, JSON_C_TO_STRING_PLAIN);
			ans_len = strlen(jsonstr);
			msg=(char*)malloc(ans_len+3); // + "\r\n\0"
			strcpy(msg, jsonstr);
			strcpy(msg+ans_len, "\r\n");
			if(!detached) printf("sending back %s\n", jsonstr);
			ans_len = send(answer_to, msg, ans_len+2, 0);
			errno_old = errno;
			json_object_put(ret);
		}else{
			if(!detached) printf("ret is NULL\n");
		}
	}else{
		if(!detached) printf("couldn't parse object: %s\n",
				json_tokener_error_desc(json_tokener_get_error(tok)));
	}
	if(obj != NULL) json_object_put(obj);
	json_tokener_free(tok);
	errno=errno_old;
	return ans_len;
}

void* client_thread(void*arg) {
	struct client_holder *client = arg;
	char buf[TCP_PACKET_LENGTH_MAX];
	int len;
    struct timeval tv;

    block_thread_signals();

	tv.tv_sec = RECV_TIMEOUT_SEC;
	tv.tv_usec = 0;
	setsockopt(client->socket, SOL_SOCKET, SO_RCVTIMEO,
			(const char*)&tv, sizeof tv);

    while(!exit_flag) {
    	len = recv(client->socket, buf, TCP_PACKET_LENGTH_MAX-1, 0);
    	if(len<0){
    		if(errno==EAGAIN || errno==EWOULDBLOCK) {
    			continue;
    		}
    		p_error("tcp client recv");
    		break;
    	}else if(len==0){ // zero-length messages are not allowed
    		if(!detached) printf("connection is closed by client\n");
    		break;
    	}else{
    		if(!detached) printf("Received TCP packet from %s:%d of len %d\n",
    				inet_ntoa(client->address.sin_addr),
					ntohs(client->address.sin_port), len);
    		buf[len]='\0';
    		if(process_control_request(client->socket, buf, len+1)<0){
    			p_error("process_control_request");
    		}
    	}
    }
    client_destroy(client);
    if(!detached) printf("removed client\n");

	return NULL;
}

int test_changes(){
	static struct state p = {};
	if(memcmp(&current_state,&p,sizeof(struct state))!=0){
		memcpy(&p,&current_state,sizeof(struct state));
		return 1;
	}
	return 0;
}

void notify_everyone(){
	struct client_holder *I;
	int len;
	struct json_object *obj, *params;
	char buf[TCP_PACKET_LENGTH_MAX];
	obj=json_object_new_object();
	params=json_object_new_object();

	json_object_object_add(obj, "method",
	    		json_object_new_string("props"));

	json_object_object_add(params, "power",
	    		json_object_new_string("on"));

	json_object_object_add(params, "bg_power",
	    		json_object_new_string("on"));

	json_object_object_add(params, "bright",
	    		json_object_new_string(current_state.main_state>128?"100":"0"));

	json_object_object_add(params, "bg_lmode",
	    		json_object_new_string("1"));

	snprintf(buf, TCP_PACKET_LENGTH_MAX, "%d", current_state.color);
	json_object_object_add(params, "bg_rgb",
	    		json_object_new_string(buf));

	json_object_object_add(obj, "params", params);

	len = snprintf(buf, TCP_PACKET_LENGTH_MAX, "%s\r\n",
			json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN));

	json_object_put(obj);

	pthread_mutex_lock(&client_holder_manipulation);
	for(I=client_holder_start;I;I=I->right){
		send(I->socket, buf, len, 0);
	}
	pthread_mutex_unlock(&client_holder_manipulation);
}

void set_tv(struct timeval* tv, double sec){
	double frac, intg;
	frac = modf(sec, &intg);
	tv->tv_sec = intg;
	tv->tv_usec = (frac*1000000.);
}

void wait_or_trigger(double poll_time){
	struct timeval tv, now;
	struct timespec ts;
	set_tv(&tv, poll_time);
	gettimeofday(&now, NULL);
	timeradd(&tv, &now, &tv);
	ts.tv_nsec = tv.tv_usec * 1000;
	ts.tv_sec = tv.tv_sec;
	pthread_mutex_lock(&task_wait_mux);
	if(pthread_cond_timedwait(&has_tasks, &task_wait_mux, &ts)<0){
		if (errno != EAGAIN){
			p_error("pthread_cond_timedwait");
			exit_flag++;
		}
	}
	pthread_mutex_unlock(&task_wait_mux);
}

void send_task(){
	struct json_object *obj, *rgb;
	struct task copy;
	obj=json_object_new_object();

	pthread_mutex_lock(&task_manipulation);
	memcpy(&copy, &current_task, sizeof(copy));
	current_task.main_state = -1;
	current_task.time = -1;
	pthread_mutex_unlock(&task_manipulation);

	if(copy.main_state>=0){
		json_object_object_add(obj, "main",
		    		json_object_new_int(copy.main_state));
	}
	if(copy.time>=0){
		rgb=json_object_new_object();
		json_object_object_add(rgb, "color",
	    		json_object_new_int(copy.color));
		json_object_object_add(rgb, "time",
	    		json_object_new_double(copy.time));
		json_object_object_add(rgb, "transition",
	    		json_object_new_int(copy.transition));
		json_object_object_add(obj, "rgb", rgb);
	}
	call_rgbdriver(json_object_to_json_string_ext(obj,
			JSON_C_TO_STRING_PLAIN));

	json_object_put(obj);
}

void* driver_commander(void*arg) {
    block_thread_signals();
    static double poll_time=RECV_TIMEOUT_SEC;

    while(!exit_flag) {
    	send_task();

    	if(test_changes()){
    		notify_everyone();
    		poll_time=POLL_TIMEOUT_MIN;
    	}else{
    		if(poll_time < RECV_TIMEOUT_SEC)
    			poll_time += POLL_TIMEOUT_INC;
    	}
    	wait_or_trigger(poll_time);
    }
    if(!detached) printf("finished driver_commander\n");

	return NULL;
}

int client_init(int client_sock, struct sockaddr_in client_addr) {
	struct client_holder *client = (struct client_holder*)malloc(sizeof(struct client_holder));
	bzero((char*)client, sizeof(struct client_holder));
	client->socket=client_sock;
	client->address=client_addr;

	pthread_create(&(client->thread), NULL, client_thread, client);
	pthread_detach(client->thread);

	client_holder_push(client);

	return 0;
}

void *tcp_server(void *vargp){
	struct sockaddr_in sin, client_addr;
	int client_sock;
	int optval = 1, len=sizeof(sin);

    block_thread_signals();

    pthread_mutexattr_init(&client_holder_manipulation_attr);
    pthread_mutexattr_settype(&client_holder_manipulation_attr,
    		PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&client_holder_manipulation,
    		&client_holder_manipulation_attr);

	tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(tcp_socket<0){
		p_error("socket tcp");
		exit(1);
	}
	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = 0;

	if (setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))){
		p_errorce("tcp setsockopt()",tcp_socket);
	}

	if (bind(tcp_socket, (struct sockaddr *)&sin, len) < 0){
		p_errorce("bind tcp",tcp_socket);
	}
	if (getsockname(tcp_socket, (struct sockaddr*) &sin, &len) != 0) {
		p_errorce("tcp getsockname()",tcp_socket);
	}
	tcp_port = ntohs(sin.sin_port);

	if (listen(tcp_socket, TCP_LISTEN_CONNECTIONS)<0){
		p_errorce("tcp listen()",tcp_socket);
	}

	pthread_cond_signal(&got_tcp_port);

	while(!exit_flag) {
		int size = sizeof(client_addr);
		client_sock = accept(tcp_socket, (struct sockaddr *)&client_addr, &size);
		if(client_sock<0){
			if(!exit_flag){
				p_error("tcp accept()");
			} // else probably the socket was just shut down.
			break;
		}else{
			printf("got client connection\n");
			client_init(client_sock, client_addr);
		}
	}
	close(tcp_socket);
	printf("closed tcp_socket\n");

	return NULL;
}

int server() {
    sigset_t set;
    time_t T;
    struct timespec t;
	sprintf(host_value, "%s:%d", MCAST_GROUP, UDP_SERVER_PORT);

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

    root_id = pthread_self();
    pthread_mutex_lock(&tcp_port_wait_locker);

    pthread_create(&driver_commander_id, NULL, driver_commander, NULL);
    pthread_create(&tcp_server_id, NULL, tcp_server, NULL);

    time(&T);
    t.tv_sec = T + 5;

	if(pthread_cond_timedwait(&got_tcp_port, &tcp_port_wait_locker, &t)<0){
        p_error("pthread_cond_timedwait got_tcp_port");
        exit(1);
	}
	if(!detached){
    	printf("TCP channel created on %s:%d\n", get_address(), tcp_port);
    }

    pthread_create(&udp_listener_id, NULL, udp_listener, NULL);
    pthread_join(udp_listener_id, NULL);

	if(shutdown(tcp_socket, SHUT_RD)<0){
		p_error("tcp shutdown");
	}

	pthread_join(tcp_server_id, NULL);
	pthread_join(driver_commander_id, NULL);
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

int main(int argc, char** argv) {
    program_name = argv[0];

    if(argc>1 && !strcmp(argv[1], "daemon")){
   		detached = 1;
    	printf("Detaching daemon...\n");
        return server_fork();
    }

    return server();
}
