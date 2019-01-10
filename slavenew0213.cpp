//#include <sys/ioctl.h>
//#include <net/if.h>
//#include <arpa/inet.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <gst/gst.h>
#include "lin.h"
typedef struct {
  GstElement *pipeline;
  GstElement *appsrc;
  GstElement *decode;
  GstElement *alsa;
  GstElement *vol;
  GMainLoop *loop;
  GstBuffer *buffer;
}SlaveApp;
static const char *GROUP_CONFIG_PATH = "/var/www/otm_group.txt";
static const char *interface = "wlan0";
static const int BC_PORT = 9903;
static const char *TMP_AUDIO_FILE = "/tmp/elytone_otm_audio_slave";
static const char *DUMMY_MSG = "NULL";
static const size_t DUMMY_LEN = 4;
static const int CHUNK = 8096;
static const int READ_SIZE = 100*1024;
static const int READY_THRESHOLD = 16*1024;

char *android_ip = "192.168.1.1";
static int group = 0;
static char masterip[20];
SlaveApp app;
FILE *wf, *rf;
PLAYER_STATUS status;
int ControlChannelSlave, DataChannelSlave;
int connection = -2;
pthread_t _sc, _dc, _ac ;
size_t recordForReady;
pthread_mutex_t _io;
int udpsocket_fd;
struct sockaddr_in udpaddr; /* address of this service */
struct sockaddr_in udpclient_addr; /* address of client    */
int readcount = 0;
int readflag = 0;
int doubleflag=0;
int resendflag = 0;
int resendonemore = 0;
int lostpacket[8];
int sumarray[8];
char bigbuf[8][10000];
int nbytes ;
int psum =0;

void UDPsocket(){
        if ((udpsocket_fd = socket(AF_INET, SOCK_DGRAM, 0)) <0) {
                perror ("socket failed");
                exit(EXIT_FAILURE);
        }
        bzero ((char *)&udpaddr, sizeof(udpaddr));
        udpaddr.sin_family = AF_INET;
        udpaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        udpaddr.sin_port = htons(4950);

       if (bind(udpsocket_fd, (struct sockaddr *)&udpaddr, sizeof(udpaddr)) <0){
                perror ("bind failed\n");
                exit(1);
        }
}
/*
void getBroadCast(struct sockaddr_in * src) { printf("getBroadCastn\n");
    static int _sd;
    static struct ifreq _data;
    strcpy(_data.ifr_ifrn.ifrn_name, interface);
    _sd = socket(PF_INET, SOCK_DGRAM, 0);
    ioctl(_sd, SIOCGIFBRDADDR, &_data);
        src->sin_addr.s_addr = ((struct sockaddr_in*)&_data.ifr_ifru.ifru_broadaddr)->sin_addr.s_addr;
        }*/
/*
void getMasterIP() {
    printf("getMasterIP()\n");
    int _sd;
    struct sockaddr_in _bcaddr;
    int _bcs = 1;
    int _group = -1;
    char msg[50];
    int len = sizeof(msg);
    int slen = sizeof(_bcaddr);
    memset(masterip, 0, sizeof(masterip));
    memset(&_bcaddr, 0, sizeof(_bcaddr));
    _sd = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(_sd, SOL_SOCKET, SO_BROADCAST, &_bcs, sizeof(int));
    _bcaddr.sin_family = AF_INET;
    _bcaddr.sin_port = htons(BC_PORT);
    getBroadCast(&_bcaddr);
    printf("[BC] Get Broadcast IP with Interface: %s\n", interface);
    bind(_sd, (struct sockaddr*)&_bcaddr, slen);

    printf("[BC] Wait Master IP from group %d\n", group);
    while(recvfrom(_sd, msg, len, 0, NULL, NULL) > 0)
    {
        sscanf(msg, "Master %d %s", &_group, masterip);
        if(_group == group)
        {
            printf("[BC] Get Master IP: %s\n", masterip);
            close(_sd);
            break;
        }
    }
}*/
/*void getGroup() {hints.ai_family
    FILE *fp = fopen(GROUP_CONFIG_PATH, "r");
    while(group < 0)
    {
        fflush(fp);
        group = fgetc(fp)-'0';
    }
    printf("Get Group: %d\n", group);
}*/
static void cb_message (GstBus *bus, GstMessage *msg, void *arg) {
  switch (GST_MESSAGE_TYPE (msg))
  {
    printf("cb_message\n");
    case GST_MESSAGE_ERROR:
    {
      GError *err;
      gchar *debug;

      gst_message_parse_error (msg, &err, &debug);
      g_print ("Error: %s\n", err->message);
      g_error_free (err);
      g_free (debug);

      gst_element_set_state (app.pipeline, GST_STATE_READY);
      break;
    }
    case GST_MESSAGE_ASYNC_DONE:
        g_print("Ready to Play\n");
        //send(ControlChannelSlave, "ReadyToPlay", 11, 0);
        status = READY;
        break;
    case GST_MESSAGE_CLOCK_LOST:
      /* Get a new clock */
      gst_element_set_state (app.pipeline, GST_STATE_PAUSED);
      gst_element_set_state (app.pipeline, GST_STATE_PLAYING);
      break;
    default:
      /* Unhandled message */
      break;
  }
}
static void read_data(GstElement*src, guint size, SlaveApp *app) {
    //printf("read_data\n");
	

    GstFlowReturn ret;
    guint _size = (size==(guint)0)?(guint)CHUNK:size;
    GstBuffer *buffer = gst_buffer_new_and_alloc(_size);
    pthread_mutex_lock(&_io);
    buffer->size = fread(buffer->data, 1, _size, rf);
    pthread_mutex_unlock(&_io);
    buffer->malloc_data = buffer->data;
    g_signal_emit_by_name(app->appsrc, "push-buffer", buffer, &ret);
    gst_buffer_unref(buffer);
    readcount++;
    if(ret != GST_FLOW_OK)
    {
        g_debug("Feed return %d for %d bytes\n", ret, buffer->size);
        return;
    }
    if(buffer->size < _size)
    {
        g_signal_emit_by_name(app->appsrc, "end-of-stream", &ret);
        return;
    }
    return;
}
void lost_connection() {
    connection = -1;
    shutdown(ControlChannelSlave, O_RDWR);
    shutdown(DataChannelSlave, O_RDWR);
    close(ControlChannelSlave);
    close(DataChannelSlave);
}
void *DataChannel(void*arg) {
    printf("*DataChannel\n");
    char _buf[10000];
    //ssize_t _rs;
    
    int length = sizeof(udpclient_addr);
    
    int count = 0;
    char datanumber[8];
    int sum = 0;
    int temp = 0;
    memset(lostpacket, 0, sizeof(lostpacket));
    
    printf("Server is ready to receive !!\n");
    printf("Can strike Cntrl-c to stop Server >>\n");
    while(1){
        temp = 0;
        sum = 0;
        memset(_buf, 0, 10000);
		
        //_rs = recv(DataChannelSlave, _buf, READ_SIZE, 0);
        nbytes =  recvfrom(udpsocket_fd, &_buf,10000, 0,(struct sockaddr*)&udpclient_addr, (socklen_t *)&length);
		printf("buf為: %c%c%c%c%c",_buf[0],_buf[1],_buf[2],_buf[3],_buf[4]);
		if(strncmp(_buf, "play", 4) == 0)
            {
                printf("enterplay\n");
                gst_element_set_state(app.pipeline, GST_STATE_PLAYING);
                status = PLAY;
            }
            else if(strncmp(_buf, "pause", 5) == 0)
            {
                if (status == PLAY)
                {
                    gst_element_set_state(app.pipeline, GST_STATE_PAUSED);
                    status = PAUSE;
                }
                else if(status == PAUSE)
                {
                    gst_element_set_state(app.pipeline, GST_STATE_PLAYING);
                    status = PLAY;
                }
            }else if(strncmp(_buf, "00", 2) == 0){
        printf("00讀取");    	
            	

        count++;
        memcpy(datanumber,_buf,10);
        
        //printf("%s \n",datanumber);
        //string to num
        printf("buf_10bit存入");
        int i;
        //把int 字串 datanumber轉成數字sum  .....不能直接用? 
        for(i=9 ; i>=2 ; i--){
                 temp = datanumber[i]-'0';
                 int j;
                 for(j=0; j<(9-i); j++){
                         temp = temp*10;
                         }
                 sum = sum+temp;
         }
         //printf("%d \n",sum);
		
		 sumarray[(sum-1)%8]=sum;//????
		 lostpacket[(sum-1)%8]=1;//1表示有收到 
		 printf("%d收到",(sum-1)%8);
		 memcpy(bigbuf[(sum-1)%8],_buf+10,sizeof(_buf)-10);
		 memset(datanumber, 0, sizeof(datanumber));
        if(nbytes < 0)
        {
            printf("data rs = -1 \n");
            lost_connection();
            pthread_exit(0);
        }

			}//else
    }
    pthread_exit(0);
}


void *AckChannel(void*arg) {
	
	printf("*ACKChannel\n");
    char Ackbuf[100];
    ssize_t ackrs;

    while(1)
    {

            memset(Ackbuf, 0, 100);
            ackrs = recv(DataChannelSlave, Ackbuf,100,0);
            if(ackrs < 0)
            {

                //printf(" ackrs rs <0 \n");
                lost_connection();
				exit(1);
                continue;
            }
            //printf("ackcommand: %s\n", Ackbuf);
            if(strncmp(Ackbuf, "resendonemore",13) == 0)
            {
                resendonemore = 1;
                char lostchar[10];
                int cha;
                for(cha=2;cha<10;cha++){
                    if(lostpacket[cha]==0){
                        lostchar[cha]='0';
                    }else{
                        lostchar[cha]='1';
                    }
                }
                write(DataChannelSlave,lostchar,sizeof(lostchar));
              printf("lostchar回傳%c",lostchar);
            }
			else if(strncmp(Ackbuf, "resendok", 8) == 0){
					resendflag=1;	
					
					switch(status)
					{
						case LOAD:
						if(recordForReady >= READY_THRESHOLD)
						{
							//printf("data case load\n");
							status = READY;
							gst_element_set_state(app.pipeline, GST_STATE_PAUSED);
						}
						case READY:
						case PLAY:
						case PAUSE:
							if(recordForReady >= READY_THRESHOLD)
                            {
								recordForReady -= READY_THRESHOLD;
								fflush(wf);
								fflush(rf);
								//printf("data case pause\n");
							}
							
							if(resendflag==1){
								char okack[2]={'d','d'};
								int k;
								for(k=0;k<8;k++){
									recordForReady +=fwrite(bigbuf[k], sizeof(char),nbytes-8, wf);
									//printf("sumarray[%d] = %d\n",k,sumarray[k]);
									
								}
								memset(bigbuf, 0, sizeof(bigbuf));
								memset(lostpacket, 0, sizeof(lostpacket));
								memset(sumarray, 0, sizeof(sumarray));
								doubleflag = 0;
								resendflag=0;
								okack[0]='o';
								okack[1]='k';
								write(ControlChannelSlave,okack,sizeof(okack));
							}
							break;
						default:
							break;
					}

		   
			}
			else if(strncmp(Ackbuf, "doubleok", 8) == 0)
            {
                doubleflag = 1 ;

                char lostchar[10];
                int cha;
                for(cha=2;cha<10;cha++){
                    if(lostpacket[cha]==0){
                        lostchar[cha]='0';
                    }else{
                        lostchar[cha]='1';
                    }
                }
                write(DataChannelSlave,lostchar,sizeof(lostchar));
                //write(DataChannelSlave,lostpacket,sizeof(lostpacket));
                //printf("doubleflag=1\n");
                printf("lostchar回傳%c",lostchar);

            }
			
        }
    
	pthread_exit(0);
}


void *SlaveChannel(void *arg) {
    printf("*SlaveChannel\n");
    struct sockaddr_in addr;
    size_t slen = sizeof(struct sockaddr_in);
    int _conn;
    char buf[TMP_BUF_SIZE];
    ssize_t rs;
    //long playtime;
    time_t sec;
    suseconds_t usec;
    int mute;
    float volume;
    ControlChannelSlave = socket(AF_INET, SOCK_STREAM, 0);
    UDPsocket();
    DataChannelSlave = socket(AF_INET, SOCK_STREAM, 0);
    memset(&addr, 0, slen);
    addr.sin_family = AF_INET;

    connection = -1;
    while(1)
    {
        if(connection < 0)
        {
            //getMasterIP();
            //masterip = "192.168.43.1";
            inet_aton(android_ip, &addr.sin_addr);
            addr.sin_port = htons(9904);
            _conn = connect(ControlChannelSlave, (struct sockaddr*)&addr, slen);
            if(_conn == 0)
            {
                addr.sin_port = htons(DATA_PORT);
                do
                {
                    _conn = connect(DataChannelSlave, (struct sockaddr*)&addr, slen);
                }
                while(_conn < 0);
                pthread_create(&_dc, NULL, DataChannel, NULL);
				pthread_create(&_ac, NULL, AckChannel, NULL);
                printf("Connect to Master[%d]: '%s'\n", group, masterip);
                connection = 1;
            }
        }
        else
        {
            memset(buf, 0, TMP_BUF_SIZE);
            rs = recv(ControlChannelSlave,     TMP_BUF_SIZE, 0);
            if(rs == 0)
            {

                //printf(" con rs <0 \n");
                lost_connection();
				exit(1);
                continue;
            } 
            printf("Command: %s\n", buf);
            if(strncmp(buf, DUMMY_MSG, DUMMY_LEN) == 0) continue;
            else if(strncmp(buf, "load", 4) == 0)
            {
                printf("enterload\n");
                wf = fopen(TMP_AUDIO_FILE, "wb");
                fflush(wf);
                rf = fopen(TMP_AUDIO_FILE, "rb");
                status = LOAD;
                recordForReady = 0;
            }
            else if(strncmp(buf, "stop", 4) == 0)
            {   printf("enterstop\n");
                gst_element_set_state(app.pipeline, GST_STATE_READY);
                fclose(wf);
                fclose(rf);
                status = STOP;
                recordForReady = 0;
                printf("recordForReady=0\n");
            }
            else if(strncmp(buf, "play", 4) == 0)
            {
				 getSysTimeFromCommand(buf, &sec, &usec);
                 WaitSysTime(sec, usec);
                 gst_element_set_state(app.pipeline, GST_STATE_PLAYING);
                    //ec = clock();
                    //printf("Play delay: %lfs\n", (double)(ec - sc)/CLOCKS_PER_SEC);
                 status = PLAY;
				
                //printf("enterplay\n");
                //sscanf(buf, "play %ld", &playtime);
                
                //gst_element_set_state(app.pipeline, GST_STATE_PLAYING);
                //status = PLAY;
            }
            else if(strncmp(buf, "pause", 5) == 0)
            {
                if (status == PLAY)
                {
					getSysTimeFromCommand(buf, &sec, &usec);
                    WaitSysTime(sec, usec);
                    gst_element_set_state(app.pipeline, GST_STATE_PAUSED);
                    status = PAUSE;
                }
                else if(status == PAUSE)
                {
					getSysTimeFromCommand(buf, &sec, &usec);
                    WaitSysTime(sec, usec);
                    gst_element_set_state(app.pipeline, GST_STATE_PLAYING);
                    status = PLAY;
                }
            }
			else if(strncmp(buf, "next", 8) == 0)
            {
			    if (status == PLAY)
                {
                    gst_element_set_state(app.pipeline, GST_STATE_PAUSED);
                    status = PAUSE;
					exit(1);
                }
                
 

                fflush(wf);
                fflush(rf);
                fclose(wf);
                fclose(rf);
                memset(bigbuf, 0, sizeof(bigbuf)); //clear
                memset(lostpacket, 0, sizeof(lostpacket));//clear
                memset(sumarray, 0, sizeof(sumarray));//clear
                wf = fopen(TMP_AUDIO_FILE, "wb");
                fflush(wf);
                rf = fopen(TMP_AUDIO_FILE, "rb");
                status = LOAD;
                printf("enterload\n");
                recordForReady = 0;
                doubleflag = 0;
                resendflag=0;

            }
            else if(strncmp(buf, "volume", 6) == 0)
            {
              sscanf(buf, "%*[^-]-%f:%*s", &volume);
              getSysTimeFromCommand(buf, &sec, &usec);
              g_object_set(app.vol, "volume", (double)volume, NULL);
            }
            else if(strncmp(buf, "mute", 4) == 0)
            {
              printf("mute\n");
              sscanf(buf, "%*[^-]-%d:%*s", &mute);
              getSysTimeFromCommand(buf, &sec, &usec);
              g_object_set(app.vol, "mute", (gboolean)mute, NULL);
            }
        }
    }
    pthread_exit(0);
}
int main(int argc, char const *argv[]) {
    GstBus *bus;
    //gchar *launch;
    status = NONE;
    gst_init(NULL, NULL);
    //getGroup();
    //UDPsocket();
    app.loop = g_main_loop_new(NULL, FALSE);

    app.pipeline = gst_parse_launch("appsrc name=src ! decodebin ! volume name =vol ! alsasink", NULL);

    app.buffer = gst_buffer_new_and_alloc(READ_SIZE);

    app.appsrc = gst_bin_get_by_name(GST_BIN(app.pipeline), "src");
    app.vol = gst_bin_get_by_name(GST_BIN(app.pipeline), "vol");
    g_signal_connect(app.appsrc, "need-data", G_CALLBACK(read_data), &app);

    bus = gst_element_get_bus(app.pipeline);
    gst_bus_add_signal_watch(bus);
    g_signal_connect(bus, "message", G_CALLBACK(cb_message), NULL);
    printf("pthread_create\n");
    pthread_create(&_sc, NULL, SlaveChannel, NULL);

    g_main_loop_run(app.loop);
    return 0;
}


