/*
 *  Intel AMT tcp redirection protocol helper functions.
 *
 *  Copyright (C) 2007 Gerd Hoffmann <kraxel@redhat.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#include "tcp.h"
#include "redir.h"

#define ISO_IMAGE "./b.iso"
static const char *state_name[] = {
    [ REDIR_NONE      ] = "NONE",
    [ REDIR_CONNECT   ] = "CONNECT",
    [ REDIR_INIT      ] = "INIT",
    [ REDIR_AUTH      ] = "AUTH",
    [ REDIR_INIT_SOL  ] = "INIT_SOL",
    [ REDIR_RUN_SOL   ] = "RUN_SOL",
    [ REDIR_INIT_IDER ] = "INIT_IDER",
    [ REDIR_RUN_IDER  ] = "RUN_IDER",
    [ REDIR_CLOSING   ] = "CLOSING",
    [ REDIR_CLOSED    ] = "CLOSED",
    [ REDIR_ERROR     ] = "ERROR",
};

static const char *state_desc[] = {
    [ REDIR_NONE      ] = "disconnected",
    [ REDIR_CONNECT   ] = "connection to host",
    [ REDIR_INIT      ] = "redirection initialization",
    [ REDIR_AUTH      ] = "session authentication",
    [ REDIR_INIT_SOL  ] = "serial-over-lan initialization",
    [ REDIR_RUN_SOL   ] = "serial-over-lan active",
    [ REDIR_INIT_IDER ] = "IDE redirect initialization",
    [ REDIR_RUN_IDER  ] = "IDE redirect active",
    [ REDIR_CLOSING   ] = "redirection shutdown",
    [ REDIR_CLOSED    ] = "connection closed",
    [ REDIR_ERROR     ] = "failure",
};

int state24=0;
int state28=0;
int state0000=0;
int state10a0=0;
unsigned int IDER_DATA_SIZE = 0x800;
/* ------------------------------------------------------------------ */

static void hexdump(const char *prefix, const unsigned char *data, size_t size)
{
    char ascii[17];
    int i;

    for (i = 0; i < size; i++) {
	if (0 == (i%16)) {
	    fprintf(stderr,"%s%s%04x:",
		    prefix ? prefix : "",
		    prefix ? ": "   : "",
		    i);
	    memset(ascii,0,sizeof(ascii));
	}
	if (0 == (i%4))
	    fprintf(stderr," ");
	fprintf(stderr," %02x",data[i]);
	ascii[i%16] = isprint(data[i]) ? data[i] : '.';
	if (15 == (i%16))
	    fprintf(stderr,"  %s\n",ascii);
    }
    if (0 != (i%16)) {
	while (0 != (i%16)) {
	    if (0 == (i%4))
		fprintf(stderr," ");
	    fprintf(stderr,"   ");
	    i++;
	};
	fprintf(stderr," %s\n",ascii);
    }
}

static ssize_t redir_write(struct redir *r, const char *buf, size_t count)
{
    int rc,i,counter;
	rc=0;
    if (r->trace)
	hexdump("out", buf, count);
	
	char*buf2=buf;
	int to_write=IDER_MAX_DATA_SIZE+IDER_DATA_HEADER_LEN;
	if(buf2[0]>0x30) {
		counter = get_counter();
		buf2[4]=counter;
	}
	printf("out: ");
	for(i=0;i<(count>40?40:count);i++){
		printf("%02X ",(unsigned char)buf[i]);
	}
	printf("\n");	
	i=count;
	while(i>0){
		rc = write(r->sock, buf2, i>to_write?to_write:i);
		i-=to_write;
		buf2+=to_write;
		if(i>0){
			to_write=IDER_MAX_DATA_SIZE;
			if(i>8192) {
				char data[]={0x54,0x00,0x00,0x01,get_counter(),0x00,0x00,0x00,0x00,0x00,0x20,0x00,0xb4,0x00,0x02,0x00,0x00,0x00,0xb0,0x58,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
				rc = write(r->sock,&data,IDER_DATA_HEADER_LEN);
			}
			else if(r->buf[9]==0x00&&r->buf[10]==0x00) { 
					char data[]={0x54,0x00,0x00,0x02,get_counter(),0x00,0x00,0x00,0x00,0x00,i/2048*8,0x00,0xb5,0x00,0x02,0x00,0x00,i/2048*8,r->buf[14],0x58,0x85,0x00,0x03,0x00,0x00,0x00,r->buf[14],0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
					rc = write(r->sock,&data,IDER_DATA_HEADER_LEN);
				} //SJEDNOTIT!! KROMĚ 0X03 NĚKDE
			else {
				
					char data[]={0x54,0x00,0x00,0x03,get_counter(),0x00,0x00,0x00,0x00,0x00,i/2048*8,0x00,0xb4,0x00,0x02,0x00,0x00,0x00,0xb0,0x58,0x85,0x00,0x03,0x00,0x00,0x00,0xb0,0x50,0x00,0x00,0x00,0x00,0x00,0x00};	
					rc = write(r->sock,&data,IDER_DATA_HEADER_LEN);
				
			}
		}
	}

    if (-1 == rc)
		snprintf(r->err, sizeof(r->err), "write(socket): %s", strerror(errno));
	
    return rc;
}

static void redir_state(struct redir *r, enum redir_state new)
{
    enum redir_state old = r->state;

    r->state = new;
    if (r->cb_state)
	r->cb_state(r->cb_data, old, new);
}

/* ------------------------------------------------------------------ */

const char *redir_state_name(enum redir_state state)
{
    const char *name = NULL;

    if (state < sizeof(state_name)/sizeof(state_name[0]))
	name = state_name[state];
    if (NULL == name)
	name = "unknown";
    return name;
}

const char *redir_state_desc(enum redir_state state)
{
    const char *desc = NULL;

    if (state < sizeof(state_desc)/sizeof(state_desc[0]))
	desc = state_desc[state];
    if (NULL == desc)
	desc = "unknown";
    return desc;
}

int redir_connect(struct redir *r)
{
    static unsigned char *defport = "16994";
    struct addrinfo ai;

    memset(&ai, 0, sizeof(ai));
    ai.ai_socktype = SOCK_STREAM;
    ai.ai_family = PF_UNSPEC;
    tcp_verbose = r->verbose;
    redir_state(r, REDIR_CONNECT);
    r->sock = tcp_connect(&ai, NULL, NULL, r->host,
			  strlen(r->port) ? r->port : defport);
    if (-1 == r->sock) {
        redir_state(r, REDIR_ERROR);
        /* FIXME: better error message */
        snprintf(r->err, sizeof(r->err), "connect failed");
	return -1;
    }
    return 0;
}

int redir_start(struct redir *r)
{
    unsigned char request[START_REDIRECTION_SESSION_LENGTH] = {
	START_REDIRECTION_SESSION, 0, 0, 0,  0, 0, 0, 0
    };
	printf("redir start\n");
    memcpy(request+4, r->type, 4);
    redir_state(r, REDIR_INIT);
    return redir_write(r, request, sizeof(request));
}

int redir_stop(struct redir *r)
{
    unsigned char request[END_REDIRECTION_SESSION_LENGTH] = {
	END_REDIRECTION_SESSION, 0, 0, 0
    };

    redir_state(r, REDIR_CLOSED);
    redir_write(r, request, sizeof(request));
    close(r->sock);
    return 0;
}

int redir_auth(struct redir *r)
{
    int ulen = strlen(r->user);
    int plen = strlen(r->pass);
    int len = 11+ulen+plen;
    int rc;
    unsigned char *request = malloc(len);

    memset(request, 0, len);
    request[0] = AUTHENTICATE_SESSION;
    request[4] = 0x01;
    request[5] = ulen+plen+2;
    request[9] = ulen;
    memcpy(request + 10, r->user, ulen);
    request[10 + ulen] = plen;
    memcpy(request + 11 + ulen, r->pass, plen);
    redir_state(r, REDIR_AUTH);
    rc = redir_write(r, request, len);
    free(request);
    return rc;
}

int redir_ider_start(struct redir *r)
{
    unsigned char request[START_IDER_REDIRECTION_LENGTH] = {
	START_IDER_REDIRECTION, 0, 0, 0,
	0, 0, 0, 0,
	MAX_TRANSMIT_BUFFER & 0xff,
	MAX_TRANSMIT_BUFFER >> 8,
	0 & 0xff,
	0 >> 8,
	5000 & 0xff,	
	5000 >> 8,
	1 & 0xff,
	1 >> 8,
	0 & 0xff,
	0 >> 8,
    };
    redir_state(r, REDIR_INIT_IDER);
    return redir_write(r, request, sizeof(request));
}

int redir_sol_start(struct redir *r)
{
    unsigned char request[START_SOL_REDIRECTION_LENGTH] = {
	START_SOL_REDIRECTION, 0, 0, 0,
	0, 0, 0, 0,
	MAX_TRANSMIT_BUFFER & 0xff,
	MAX_TRANSMIT_BUFFER >> 8,
	TRANSMIT_BUFFER_TIMEOUT & 0xff,
	TRANSMIT_BUFFER_TIMEOUT >> 8,
	TRANSMIT_OVERFLOW_TIMEOUT & 0xff,	TRANSMIT_OVERFLOW_TIMEOUT >> 8,
	HOST_SESSION_RX_TIMEOUT & 0xff,
	HOST_SESSION_RX_TIMEOUT >> 8,
	HOST_FIFO_RX_FLUSH_TIMEOUT & 0xff,
	HOST_FIFO_RX_FLUSH_TIMEOUT >> 8,
	HEARTBEAT_INTERVAL & 0xff,
	HEARTBEAT_INTERVAL >> 8,
	0, 0, 0, 0
    };

    redir_state(r, REDIR_INIT_SOL);
    return redir_write(r, request, sizeof(request));
}

int redir_sol_stop(struct redir *r)
{
    unsigned char request[END_SOL_REDIRECTION_LENGTH] = {
	END_SOL_REDIRECTION, 0, 0, 0,
	0, 0, 0, 0,
    };

    redir_state(r, REDIR_CLOSING);
    return redir_write(r, request, sizeof(request));
}

int redir_sol_send(struct redir *r, unsigned char *buf, int blen)
{
    int len = 10+blen;
    int rc;
    unsigned char *request = malloc(len);

    memset(request, 0, len);
    request[0] = SOL_DATA_TO_HOST;
    request[8] = blen & 0xff;
    request[9] = blen >> 8;
    memcpy(request + 10, buf, blen);
    rc = redir_write(r, request, len);
    free(request);
    return rc;
}

int redir_enable_features(struct redir *r, int fid)
{
    int len = 9;
    int rc;
   
	if(fid==0x03) len=13;
	 unsigned char *request = malloc(len);
    memset(request, 0, len);
    request[0] = IDER_DISABLE_ENABLE_FEATURES;
    request[8] = fid;
	if(fid==0x03) request[9]=0x09;
    //memcpy(request + 10, buf, blen);
    rc = redir_write(r, request, len);
    free(request);
    return rc;
}
int redir_handle_reset(struct redir *r){
	int len = 8;
    int rc;
    unsigned char *request = malloc(len);
    memset(request, 0, len);
    request[0] = IDER_RESET_OCCURED_RESPONSE;
    request[4] = r->buf[4]; //necessary??
    rc = redir_write(r, request, len);
    free(request);
    return rc;
}
int redir_sol_recv(struct redir *r)
{
    unsigned char msg[64];
    int count, len, bshift;
    int flags;

    len = r->buf[8] + (r->buf[9] << 8);
    count = r->blen - 10;
    if (count > len)
	count = len;
    bshift = count + 10;
    if (r->cb_recv)
	r->cb_recv(r->cb_data, r->buf + 10, count);
    len -= count;

    while (len) {
	if (r->trace)
	    fprintf(stderr, "in+: need %d more data bytes\n", len);
	count = sizeof(msg);
	if (count > len)
	    count = len;
	/* temporarily switch to blocking.  the actual data may not be
	   ready yet, but should be here Real Soon Now. */
	flags = fcntl(r->sock,F_GETFL);
	fcntl(r->sock,F_SETFL, flags & (~O_NONBLOCK));
	count = read(r->sock, msg, count);
	fcntl(r->sock,F_SETFL, flags);

	switch (count) {
	case -1:
	    snprintf(r->err, sizeof(r->err), "read(socket): %s", strerror(errno));
	    return -1;
	case 0:
	    snprintf(r->err, sizeof(r->err), "EOF from socket");
	    return -1;
	default:
	    if (r->trace)
		hexdump("in+", msg, count);
	    if (r->cb_recv)
		r->cb_recv(r->cb_data, msg, count);
	    len -= count;
	}
    }

    return bshift;
}
char *copy_array(int start, int len, char *request, char data[]){
	int i;
	for (i=start;i<start+len;i++){
		request[i]=data[i-start];
	}
	return request;

}
int get_counter(void){
	static int counter=0;
	counter+=1;
	counter%=255;
	return counter-1;
}
char* put_file_size(char *request, int len, char * fileName,int blocksize){
	FILE *file;
	file = fopen(fileName, "rb");
	fseek(file,0,SEEK_END);
	int fileLen=ftell(file)/blocksize-1;
	request[34]=(fileLen>>24)&0xFF;
	request[35]=(fileLen>>16)&0xFF;
	request[36]=(fileLen>>8)&0xFF;
	request[37]=fileLen&0xFF;
	fclose(file);
	return request;
}
char *  load_data_iso(char *request, int len, int part, char * fileName){
	FILE *file;

	file = fopen(fileName, "rb");
	if (!file)	{
		fprintf(stderr, "Unable to open file %s", fileName);
		return 0;
	}
	
	fseek(file,  part*IDER_DATA_SIZE,0);

	//Allocate memory
	request=(char *)realloc(request,len+IDER_DATA_HEADER_LEN);
	if (!request)
	{
		fprintf(stderr, "Memory error!");
        fclose(file);
		return 0;
	}

	//Read file contents into buffer
	fread(request+IDER_DATA_HEADER_LEN, 1,len, file);
	fclose(file);

	return request;

}

int ider_command_handle(struct redir *r){
	
	int i;
	if(r->buf[12] != 0x28) state28=0;
	int len= 31;
	//int rc;
	unsigned char *request = malloc(len);
	memset(request, 0, len);
	if((r->buf[14]==0xb0&&r->buf[15]==0xa0)||(r->buf[14]==0xa0&&r->buf[15]==0xa0)||(r->buf[14]==0x10&&r->buf[15]==0xa0)){
		request[3] = 0x02;
		if(r->buf[12] == 0x00||r->buf[12] == 0xff){
			if(r->buf[13] == 0x00){

				if (r->buf[4]<0x10||(state0000!=0&&r->buf[14]==0xa0)) {
					char data[]={0x51,0x00,0x00,0x02,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x87,0x60,0x03,0x00,0x00,0x00,r->buf[14],0x51,0x06,0x28,0x00};
					
					request=copy_array(0,len,request,data);
				}
				else {
					if(r->buf[14]==0xa0) state0000=1;
					char data[]={0x51,0x00,0x00,0x02,0x21,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xc5,0x00,0x03,0x00,0x00,0x00,r->buf[14],0x50,0x00,0x00,0x00 };
					request=copy_array(0,len,request,data);
				}
				//char data[]={0x51,0x00,0x00,0x02,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x87,0x20,0x03,0x00,0x00,0x00,0xb0,0x51,0x02,0x3a,0x00};
				//request=copy_array(0,len,request,data);
			}
			else if(r->buf[13] == 0x01){
				char data[]={0x51,0x00,0x00,0x02,0x21,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x87,0x50,0x03,0x00,0x00,0x00,0xb0,0x51,0x05,0x20,0x00};
				request=copy_array(0,len,request,data);
			}
			else if(r->buf[13] == 0x08&&r->buf[16]==0x43){
				len=46;
				request = realloc(request,len);
				char data[]={0x54,0x00,0x00,0x02,0x82,0x00,0x00,0x00,0x00,0x0c,0x00,0x00,0xb5,0x00,0x02,0x00,0x0c,0x00,0xb0,0x58,0x85,0x00,0x03,0x00,0x00,0x00,0xb0,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0a,0x01,0x01,0x00,0x14,0x01,0x00,0x00,0x00,0x00,0x00};
				request=copy_array(0,len,request,data);
			}
			else if(1){//*******SENDING DATA *************
				unsigned int pos;
				pos = (r->buf[18]<<24)+(r->buf[19]<<16)+(r->buf[20]<<8)+r->buf[21];
				if(r->buf[14]==0xa0) IDER_DATA_SIZE=512;
				else IDER_DATA_SIZE=2048;
				len=IDER_DATA_SIZE*r->buf[13]/0x08;
				//len=IDER_DATA_SIZE*r->buf[24]; //experimental //hopefully OK, try it ASAP
				if(r->buf[13]==0xff) len = IDER_DATA_SIZE*r->buf[24];

				if(r->buf[14]==0xb0||r->buf[13]==0xff) request=load_data_iso(request, len, pos, ISO_IMAGE); //posílam iso i při bootu, posílat FD místo toho? (v případě povolení bootování z FD v BIOSu bych pravděpodobně posílal CD)
				else request=load_data_iso(request, len, pos, "./fdboot.img");
				len+=IDER_DATA_HEADER_LEN;	
				if(r->buf[13]==0xff) {
					r->buf[13]=0x08;
					r->buf[14]=0xb0;
				}
				if(r->buf[24]>4&&(r->buf[9]==0x00&&r->buf[10]==0x00)) {
					printf("if before restart than be aware!!\n\n");
					char data[]={0x54,0x00,0x00,0x00,0x9c,0x00,0x00,0x00,0x00,0x00,0x20,0x00,0xb5,0x00,0x02,0x00,0x00,0x20,r->buf[14],0x58,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
					request=copy_array(0,IDER_DATA_HEADER_LEN,request,data);
				}
				else if(r->buf[24]>4) {
					char data[]={0x54,0x00,0x00,0x01,0x1d,0x00,0x00,0x00,0x00,0x00,0x20,0x00,0xb4,0x00,0x02,0x00,0x00,0x00,r->buf[14],0x58,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
					request=copy_array(0,IDER_DATA_HEADER_LEN,request,data);
				}
				else if(r->buf[9]==0x00&&r->buf[10]==0x00) { 
					char data[]={0x54,0x00,0x00,0x02,0x83,0x00,0x00,0x00,0x00,0x00,r->buf[14]==0xa0?0x02:r->buf[13],0x00,0xb5,0x00,0x02,0x00,0x00,r->buf[14]==0xa0?0x02:r->buf[13],r->buf[14],0x58,0x85,0x00,0x03,0x00,0x00,0x00,r->buf[14],0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
					request=copy_array(0,IDER_DATA_HEADER_LEN,request,data);
				}
				else{
					char data[]={0x54,0x00,0x00,0x03,0x1d,0x00,0x00,0x00,0x00,0x00,r->buf[13],0x00,0xb4,0x00,0x02,0x00,0x00,0x00,r->buf[14],0x58,0x85,0x00,0x03,0x00,0x00,0x00,r->buf[14],0x50,0x00,0x00,0x00,0x00,0x00,0x00};
					request=copy_array(0,IDER_DATA_HEADER_LEN,request,data);
				}
			}

		}
		else if(r->buf[12] == 0x10){
			char data[]={0x51,0x00,0x00,0x02,0x67,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x87,0x50,0x03,0x00,0x00,0x00,0xb0,0x51,0x05,0x20,0x00};
			request=copy_array(0,len,request,data);
		}
		else if(r->buf[12] == 0x08){ //velikost ISO!!
			int type;
			if(r->buf[14]==0xb0) type=2048;
			else type=512;
			len=42;
			request = realloc(request,len);
			char data[]={0x54,0x00,0x00,0x02,0x1a,0x00,0x00,0x00,0x00,0x08,0x00,0x00,0xb5,0x00,0x02,0x00,0x08,0x00,r->buf[14],0x58,0x85,0x00,0x03,0x00,0x00,0x00,r->buf[14],0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0c,0x58,0x00,0x00,0x08,0x00 };
			request=copy_array(0,len,request,data);
			if(r->buf[14]==0xb0) request=put_file_size(request, len,ISO_IMAGE,type);
			else request=put_file_size(request, len,"fdboot.img",type);
		}
		else if(r->buf[12] == 0x0c){ 
			len=46;
			request = realloc(request,len);
			char data[]={0x54,0x00,0x00,0x02,0x20,0x00,0x00,0x00,0x00,0x0c,0x00,0x00,0xb5,0x00,0x02,0x00,0x0c,0x00,0xb0,0x58,0x85,0x00,0x03,0x00,0x00,0x00,0xb0,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0a,0x01,0x01,0x00,0x14,0x01,0x00,0x00,0x00,0x00,0x00};
			request=copy_array(0,len,request,data);
		}
		else if(r->buf[12] == 0x28){
			if(r->buf[14]==0xa0){
				len=74;
				char data[]={0x54,0x00,0x00,0x02,0xb9,0x00,0x00,0x00,0x00,0x28,0x00,0x00,0xb5,0x00,0x02,0x00,0x28,0x00,0xa0,0x58,0x85,0x00,0x03,0x00,0x00,0x00,0xa0,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x26,0x24,0x00,0x00,0x00,0x00,0x00,0x05,0x1e,0x04,0xb0,0x02,0x12,0x02,0x00,0x00,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x28,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0xd0,0x00,0x00};
				request = realloc(request,len);
				request=copy_array(0,len,request,data);
			}
			else if(state28==0){
				len=50;
				char data[]={0x54,0x00,0x00,0x02,0x0a,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0xb5,0x00,0x02,0x00,0x10,0x00,r->buf[14],0x58,0x85,0x00,0x03,0x00,0x00,0x00,r->buf[14],0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0c,0x00,0x00,0x00,0x08,0x00,0x00,0x03,0x04,0x00,0x08,0x01,0x00};
				state28++;
				request = realloc(request,len);
				request=copy_array(0,len,request,data);
			}
			else if(state28==1){
				len=46;
				char data[]={0x54,0x00,0x00,0x02,0x0a,0x00,0x00,0x00,0x00,0x0c,0x00,0x00,0xb5,0x00,0x02,0x00,0x0c,0x00,0xb0,0x58,0x85,0x00,0x03,0x00,0x00,0x00,0xb0,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x08,0x00,0x1e,0x03,0x00};
				state28++;
				request = realloc(request,len);
				request=copy_array(0,len,request,data);
			}
			else{
				len=42;
				char data[]={0x54,0x00,0x00,0x02,0x0b,0x00,0x00,0x00,0x00,0x08,0x00,0x00,0xb5,0x00,0x02,0x00,0x08,0x00,0xb0,0x58,0x85,0x00,0x03,0x00,0x00,0x00,0xb0,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x08};
				request = realloc(request,len);
				request=copy_array(0,len,request,data);
			}	
			
		}
		else if(r->buf[12] == 0x24){ 
			
			if(state24==-1){
				len=54;
				char data[]={0x54,0x00,0x00,0x02,0x19,0x00,0x00,0x00,0x00,0x14,0x00,0x00,0xb5,0x00,0x02,0x00,0x14,0x00,0xb0,0x58,0x85,0x00,0x03,0x00,0x00,0x00,0xb0,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x12,0x01,0x01,0x00,0x14,0x01,0x00,0x00,0x00,0x02,0x00,0x00,0x14,0xaa,0x00,0x00,0x00,0x2c,0x0c};
				request = realloc(request,len);
				request=copy_array(0,len,request,data);
			}
			else if(state24==1){
				len = 46;char data[]={0x54,0x00,0x00,0x02,0x27,0x00,0x00,0x00,0x00,0x0c,0x00,0x00,0xb5,0x00,0x02,0x00,0x0c,0x00,0xb0,0x58,0x85,0x00,0x03,0x00,0x00,0x00,0xb0,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0a,0x01,0x01,0x00,0x14,0x01,0x00,0x00,0x00,0x00,0x00};
				state24=-1;
				request = realloc(request,len);
				request=copy_array(0,len,request,data);
			}
			else if(state24==0){
				len=54;
				char data[]={0x54,0x00,0x00,0x02,0x19,0x00,0x00,0x00,0x00,0x14,0x00,0x00,0xb5,0x00,0x02,0x00,0x14,0x00,0xb0,0x58,0x85,0x00,0x03,0x00,0x00,0x00,0xb0,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x12,0x01,0x01,0x00,0x14,0x01,0x00,0x00,0x00,0x02,0x00,0x00,0x14,0xaa,0x00,0x00,0x00,0x04,0x1a};
				state24=1;
				request = realloc(request,len);
				request=copy_array(0,len,request,data);
			}
			
			
			//len = 46;char data[]={0x54,0x00,0x00,0x02,0x27,0x00,0x00,0x00,0x00,0x0c,0x00,0x00,0xb5,0x00,0x02,0x00,0x0c,0x00,0xb0,0x58,0x85,0x00,0x03,0x00,0x00,0x00,0xb0,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0a,0x01,0x01,0x00,0x14,0x01,0x00,0x00,0x00,0x00,0x00};
			
			
		}
		else if(r->buf[12] == 0x2e){	
			char data[]={0x51,0x00,0x00,0x02,0x4e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x87,0x50,0x03,0x00,0x00,0x00,0xa0,0x51,0x05,0x24,0x00};
			request=copy_array(0,len,request,data);
		
		}
		else if(r->buf[12] == 0x40){
			len=98;
			request = realloc(request,len);
			char data[]={0x54,0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0xb5,0x00,0x02,0x00,0x40,0x00,0xb0,0x58,0x85,0x00,0x03,0x00,0x00,0x00,0xb0,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3c,0x00,0x00,0x00,0x08,0x00,0x00,0x03,0x04,0x00,0x08,0x01,0x00,0x00,0x01,0x03,0x04,0x00,0x00,0x00,0x02,0x00,0x02,0x03,0x04,0x00,0x00,0x00,0x00,0x00,0x03,0x03,0x04,0x29,0x00,0x00,0x02,0x00,0x10,0x01,0x08,0x00,0x00,0x08,0x00,0x00,0x01,0x00,0x00,0x00,0x1e,0x03,0x00,0x01,0x00,0x03,0x00,0x01,0x05,0x03,0x00} ;
			request=copy_array(0,len,request,data);

			
		}
		else if(r->buf[12] == 0x54){ 
			len=54;
			request = realloc(request,len);
			char data[]={0x54,0x00,0x00,0x02,0x28,0x00,0x00,0x00,0x00,0x14,0x00,0x00,0xb5,0x00,0x02,0x00,0x14,0x00,0xb0,0x58,0x85,0x00,0x03,0x00,0x00,0x00,0xb0,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x12,0x01,0x01,0x00,0x14,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x14,0xaa,0x00,0x00,0x00,0x00,0x00};
			request=copy_array(0,len,request,data);
		}
		else if(r->buf[12] == 0x88){
			char data[]={0x51,0x00,0x00,0x02,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x87,0x50,0x03,0x00,0x00,0x00,0xb0,0x51,0x05,0x20,0x00};
			request=copy_array(0,len,request,data);
		}
		
		
		else if(r->buf[12] == 0xf0){
			len=68;
			request = realloc(request,len);
			char data[]={0x54,0x00,0x00,0x02,0x05,0x00,0x00,0x00,0x00,0x22,0x00,0x00,0xb5,0x00,0x02,0x00,0x22,0x00,0xb0,0x58,0x85,0x00,0x03,0x00,0x00,0x00,0xb0,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x01,0x80,0x00,0x00,0x00,0x00,0x2a,0x18,0x00,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 };
			request=copy_array(0,len,request,data);
		}
		else{
			printf("UNKNOWN!!\n\n\n\n\n");
			request[0] = IDER_COMMAND_END_RESPONSE;
			char data[]={0x87,0x60,0x03,0x00,0x00,0x00,0xb0,0x51,0x06,0x28,0x00};
			request=copy_array(20,11,request,data);
		}	
	}
	else if(r->buf[15] ==0xa0){
		if(r->buf[12] == 0x10){
			if(r->buf[14] == 0x00){
				switch (state10a0){
					case 0:{
						len=50;
						request = realloc(request,len);
						char data[]={0x54,0x00,0x00,0x02,0x06,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0xb5,0x00,0x02,0x00,0x10,0x00,0xa0,0x58,0x85,0x00,0x03,0x00,0x00,0x00,0xa0,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0c,0x00,0x00,0x00,0x08,0x00,0x00,0x03,0x04,0x00,0x08,0x01,0x00};
						request=copy_array(0,len,request,data);
						break;
					}
					case 1:{
						len=50;
						request = realloc(request,len);
						char data[]={0x54,0x00,0x00,0x02,0x6e,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0xb5,0x00,0x02,0x00,0x10,0x00,0xa0,0x58,0x85,0x00,0x03,0x00,0x00,0x00,0xa0,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0c,0x00,0x00,0x00,0x08,0x00,0x03,0x03,0x04,0x29,0x00,0x00,0x02};
						request=copy_array(0,len,request,data);
						break;
					}
					default:	break;
				}
			}
			if(r->buf[14] == 0x10){
				len=50;
				request = realloc(request,len);
				char data[]={0x54,0x00,0x00,0x02,0x6f,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0xb5,0x00,0x02,0x00,0x10,0x00,0xb0,0x58,0x85,0x00,0x03,0x00,0x00,0x00,0xb0,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0c,0x00,0x00,0x00,0x08,0x00,0x00,0x03,0x04,0x00,0x08,0x01,0x00};
				request=copy_array(0,len,request,data);
				
			}
		}
		
		else if(r->buf[12] == 0x00){
			if(r->buf[13] == 0x00){
				
				char data[]={0x51,0x00,0x00,0x02,0x71,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x87,0x60,0x03,0x00,0x00,0x00,0xa0,0x51,0x06,0x28,0x00};
				request=copy_array(0,len,request,data);
			}
			if(r->buf[13] == 0x01){
				if(r->buf[16] == 0x23){
					len=46;
					request = realloc(request,len);
					char data[]={0x54,0x00,0x00,0x02,0x0b,0x00,0x00,0x00,0x00,0x0c,0x00,0x00,0xb5,0x00,0x02,0x00,0x0c,0x00,0xa0,0x58,0x85,0x00,0x03,0x00,0x00,0x00,0xa0,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x01,0x02,0x00,0x02,0x00};
					request=copy_array(0,len,request,data);
				}
				else{ //potentionally some data??
					len=128;
					request = realloc(request,len);
					char data[]={0x54,0x00,0x00,0x02,0x0c,0x00,0x00,0x00,0x00,0x5e,0x00,0x00,0xb5,0x00,0x02,0x00,0x5e,0x00,0xa0,0x58,0x85,0x00,0x03,0x00,0x00,0x00,0xa0,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x5c,0x24,0x00,0x00,0x00,0x00,0x00,0x01,0x0a,0x00,0x01,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x03,0x16,0x00,0xa0,0x00,0x00,0x00,0x00,0x00,0x12,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xa0,0x00,0x00,0x00,0x05,0x1e,0x04,0xb0,0x02,0x12,0x02,0x00,0x00,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x28,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0xd0,0x00,0x00,0x08,0x0a,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0b,0x06,0x00,0x00,0x00,0x11,0x24,0x31};
					request=copy_array(0,len,request,data);
				}
			}
		}
		else if(r->buf[12] == 0xff){ //sendind 512B of data
			len=IDER_DATA_HEADER_LEN+512;
			request = realloc(request,len);
			char data[]={0x54,0x00,0x00,0x03,0x0d,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0xb4,0x00,0x02,0x00,0x00,0x00,0xa0,0x58,0x85,0x00,0x03,0x00,0x00,0x00,0xa0,0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
			request=copy_array(0,len,request,data);
			//len=512*r->buf[13]/0x08;
			//printf("pos: %i\n",pos);
			request=load_data_iso(request, 512, 0, "./fdboot.img");
			request=copy_array(0,IDER_DATA_HEADER_LEN,request,data);

		}
		
		
		else{
			for(i=0;i<r->blen;i++){
				printf("%02X\n",r->buf[i]);
			}
			printf("\n");
		}
		
	}
	else{
			for(i=0;i<r->blen;i++){
				printf("%02X\n",r->buf[i]);
			}
			printf("\n");
		}
	
	
	redir_write(r, request, len);
	return 0;
}

static int in_loopback_mode = 0;
static int powered_off = 0;

int redir_data(struct redir *r)
{
    int rc, bshift, i;
	
	
    if (r->trace) {
	fprintf(stderr, "in --\n");
	if (r->blen)
	    fprintf(stderr, "in : already have %d\n", r->blen);
    }
	
    rc = read(r->sock, r->buf + r->blen, sizeof(r->buf) - r->blen);
	
	

    switch (rc) {
		case -1:
			snprintf(r->err, sizeof(r->err), "read(socket): %s", strerror(errno));
			goto err;
		case 0:
			snprintf(r->err, sizeof(r->err), "EOF from socket");
			goto err;
		default:
		if (r->trace)
			hexdump("in ", r->buf + r->blen, rc);
		r->blen += rc;
    }
	printf ("in:  ");
	for(i=0;i<rc;i++){
		printf("%02X ",r->buf[i]);
	}
	printf("\n");
    for (;;) {
	if (r->blen < 4)
	    goto again;
	bshift = 0;
	switch (r->buf[0]) {
	case START_REDIRECTION_SESSION_REPLY:
	    bshift = START_REDIRECTION_SESSION_REPLY_LENGTH;
	    if (r->blen < bshift)
		goto again;
	    if (r->buf[1] != STATUS_SUCCESS) {
		snprintf(r->err, sizeof(r->err), "redirection session start failed");
		goto err;
	    }
	    if (-1 == redir_auth(r))
		goto err;
	    break;
	case AUTHENTICATE_SESSION_REPLY:
	    bshift = r->blen; /* FIXME */
	    if (r->blen < bshift)
		goto again;
	    if (r->buf[1] != STATUS_SUCCESS) {
		snprintf(r->err, sizeof(r->err), "session authentication failed");
		goto err;
	    }
		if(r->type[0]=='I'){
			if (-1 == redir_ider_start(r))
			goto err;
			break;
		}
	    if (-1 == redir_sol_start(r))
		goto err;
	    break;
	case START_SOL_REDIRECTION_REPLY:
	    bshift = r->blen; /* FIXME */
	    if (r->blen < bshift)
		goto again;
	    if (r->buf[1] != STATUS_SUCCESS) {
		snprintf(r->err, sizeof(r->err), "serial-over-lan redirection failed");
		goto err;
	    }
	    redir_state(r, REDIR_RUN_SOL);
	    break;
	case SOL_HEARTBEAT:
	case SOL_KEEP_ALIVE_PING:
	case IDER_HEARTBEAT:
	case IDER_KEEP_ALIVE_PING:
	    bshift = HEARTBEAT_LENGTH;
	    if (r->blen < bshift)
		goto again;
	    if (HEARTBEAT_LENGTH != redir_write(r, r->buf, HEARTBEAT_LENGTH))
		goto err;
	    break;
	case SOL_DATA_FROM_HOST:
	    if (r->blen < 10) /* header length */
		goto again;
	    bshift = redir_sol_recv(r);
	    if (bshift < 0)
		goto err;
	    break;
	case END_SOL_REDIRECTION_REPLY:
	    bshift = r->blen; /* FIXME */
	    if (r->blen < bshift)
		goto again;
	    redir_stop(r);
	    break;
	case SOL_CONTROLS_FROM_HOST: {
	  bshift = r->blen; /* FIXME */
	  if (r->blen < bshift)
	    goto again;
	  
	  /* Host sends this message to the Management Console when
	   * the host has changed its COM port control lines. This
	   * message is likely to be one of the first messages that
	   * the Host sends to the Console after it starts SOL
	   * redirection.
	   */
	  struct controls_from_host_message *msg = (struct controls_from_host_message *) r->buf;
	  //printf("Type %x, control %d, status %d\n", msg->type, msg->control, msg->status);
	  if (msg->status & LOOPBACK_ACTIVE) {
	    if (r->verbose)
	      fprintf (stderr, "Warning, SOL device is running in loopback mode.  Text input may not be accepted\n");
	    in_loopback_mode = 1;
	  } else if (in_loopback_mode) {
	    if (r->verbose)
	      fprintf (stderr, "SOL device is no longer running in loopback mode\n");
	    in_loopback_mode = 0;
	  }

	  if (0 == (msg->status & SYSTEM_POWER_STATE))  {
	    if (r->verbose)
	      fprintf (stderr, "The system is powered off.\n");
	    powered_off = 1;
	  } else if (powered_off) {
	    if (r->verbose)
	      fprintf (stderr, "The system is powered on.\n");
	    powered_off = 0;
	  }
	  
	  if (r->verbose) {
	    if (msg->status & (TX_OVERFLOW|RX_FLUSH_TIMEOUT|TESTMODE_ACTIVE))
	      fprintf (stderr, "Other unhandled status condition\n");
	    
	    if (msg->control & RTS_CONTROL) 
	      fprintf (stderr, "RTS is asserted on the COM Port\n");
	    
	    if (msg->control & DTR_CONTROL) 
	      fprintf (stderr, "DTR is asserted on the COM Port\n");
	    
	    if (msg->control & BREAK_CONTROL) 
	      fprintf (stderr, "BREAK is asserted on the COM Port\n");
	  }

	  break;
	}
	case START_IDER_REDIRECTION_REPLY:{
	//printf("aha, sem vůl\n");
		bshift = r->blen;
		redir_enable_features(r,1);
		redir_state(r, REDIR_INIT_IDER_2);
		break;
	}
	case IDER_DISABLE_ENABLE_FEATURES_REPLY:{
		bshift = r->blen;
	//printf("%i\n",r->state);
		switch(r->state){
			case REDIR_INIT_IDER_1:{
				redir_enable_features(r,2);
				redir_state(r, REDIR_RUN_IDER);
				break;
			}
			case REDIR_INIT_IDER_2:{
				redir_enable_features(r,3);
				redir_state(r, REDIR_INIT_IDER_4);
				break;
			}
			case REDIR_INIT_IDER_4:{
				redir_enable_features(r,5);
				redir_state(r, REDIR_INIT_IDER_5);
				break;
			}
			case REDIR_INIT_IDER_5:{
				redir_state(r, REDIR_RUN_IDER);
				break;
			}
			default:
				break;
		}	
		break;
	}
	case IDER_COMMAND_WRITTEN:{
		ider_command_handle(r);
		bshift = r->blen;
		break;
	}
	case IDER_RESET_OCCURED:{
		printf("reset\n");
		redir_handle_reset(r);
		bshift = r->blen;
		break;
	}
	case IDER_AFTER_RESET:{ //přejmenovat! jedná se o reakci na špatná data která mu zasílám, obvykle se tento požadavek rovná konci
		printf("unknown demand\n");
		int len = 8;
		//int rc;
		unsigned char *request = malloc(len);
		memset(request, 0, len);
		request[0] = IDER_AFTER_RESET;
		rc = redir_write(r, request, len);
		free(request);
		bshift = r->blen;
		break;
	}
	
	
		
	default:
		printf("unknown command 0x%x\n",r->buf[0]);
		int i=0;
		for(i=0;i<r->blen;i++){
			printf("%x ",r->buf[i]);
		}
		printf("\n");
	    snprintf(r->err, sizeof(r->err), "%s: unknown r->buf 0x%02x",
		     __FUNCTION__, r->buf[0]);
	    goto err;
	}

	if (bshift == r->blen) {
	    r->blen = 0;
	    break;
	}

	/* have more data, shift by bshift */
	if (r->trace)
	    fprintf(stderr, "in : shift by %d\n", bshift);
	memmove(r->buf, r->buf + bshift, r->blen - bshift);
	r->blen -= bshift;
    }
    return 0;

again:
    /* need more data, jump back into poll/select loop */
    if (r->trace)
	fprintf(stderr, "in : need more data\n");
    return 0;

err:
    if (r->trace)
	fprintf(stderr, "in : ERROR (%s)\n", r->err);
    redir_state(r, REDIR_ERROR);
    close(r->sock);
    return -1;
}