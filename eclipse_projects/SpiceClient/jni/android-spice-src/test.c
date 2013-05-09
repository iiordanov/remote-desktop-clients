/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2011  Keqisoft,Co,Ltd.
   Copyright (C) 2011  Shuxiang Lin (shohyanglim@gmail.com)

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
/* a client in the unix domain */
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include "android-spice.h"
#include "android-spice-priv.h"
pthread_mutex_t android_mutex = PTHREAD_MUTEX_INITIALIZER;  
pthread_cond_t android_cond = PTHREAD_COND_INITIALIZER;  
volatile int android_task_ready;

void error(const char *);
int test_input();
int test_output();

void getval(void* dest,void* src,int type)
{
    switch(type)
    {
	case INT:
	    memset(dest,0,4);
	    memcpy(dest,src,4);
	    break;
	case UINT8:
	    memset(dest,0,1);
	    memcpy(dest,src,1);
	    break;
    }
}

int main()
{
    pthread_t android_input, android_output;  
    int  iret1, iret2;  
    iret1 = pthread_create( &android_input, NULL, (void*)test_input, NULL);  
    iret2 = pthread_create( &android_output, NULL, (void*)test_output, NULL);  

    pthread_join(android_input, NULL);  
    pthread_join(android_output, NULL);   

    return 0;
}
int test_input()
{
    int sockfd, servlen,n;
    struct sockaddr_un  serv_addr;
    char buffer[82];

    bzero((char *)&serv_addr,sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    //char* sock = "/data/local/tmp/spice-input.socket";
    char* sock = "/data/data/com.iiordanov.aSPICE/aspice-input-socket.socket";
    strcpy(serv_addr.sun_path, sock);
    servlen = strlen(serv_addr.sun_path) + 
	sizeof(serv_addr.sun_family);
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM,0)) < 0)
	error("Creating socket");
    if (connect(sockfd, (struct sockaddr *) 
		&serv_addr, servlen) < 0)
	error("Connecting");
    printf("connection has been set\n");
    bzero(buffer,82);
    AndroidEventKey msg;
    msg.type = ANDROID_OVER;
    memcpy(buffer,&msg,4);

    while(!android_task_ready);  
    pthread_mutex_lock(&android_mutex);  
    n = write(sockfd,buffer,sizeof(AndroidEventKey));
    pthread_cond_signal(&android_cond);  
    pthread_mutex_unlock(&android_mutex);  
 
    if(n==0)
    {
	printf("connection has been closed\n");
    }
    close(sockfd);
    return 0;
}

int test_output()
{
    int sockfd, servlen,n;
    int pics = 5;
    int type,size;
    struct sockaddr_un  serv_addr;
    char* buffer;
    char buf[16];

    bzero((char *)&serv_addr,sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    //char* sock = "/data/local/tmp/spice-output.socket";
    char* sock = "/data/data/com.iiordanov.aSPICE/aspice-output-socket.socket";
    strcpy(serv_addr.sun_path, sock);
    servlen = strlen(serv_addr.sun_path) + 
	sizeof(serv_addr.sun_family);
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM,0)) < 0)
	error("Creating socket");
    if (connect(sockfd, (struct sockaddr *) 
		&serv_addr, servlen) < 0)
	error("Connecting");
    printf("connection has been set\n");
    AndroidShow* android_show = (AndroidShow*)malloc(sizeof(AndroidShow));
    //while(pics)
    //{
    n = read(sockfd,buf,4);
    if(n==4)
    {
	getval(&type,buf,INT);
	if(type==ANDROID_SHOW)
	{
	    n = read(sockfd,buf,16);
	    if(n==16)
	    {
		android_show->type = type;
		getval(&android_show->width,buf,INT);
		getval(&android_show->height,buf+4,INT);
		getval(&android_show->x,buf+8,INT);
		getval(&android_show->y,buf+12,INT);
		size =android_show->width*android_show->height; 
		buffer = (uint8_t*)malloc(size*4);
		n = read(sockfd,buffer,size*4);
		if(n == size*4)
		    write_ppm_32(buffer,android_show->width,android_show->height);
		else 
		    goto error;
		fprintf(stderr,"Image bytes got:%d\n",n);
		free(buffer);
	    }
	    else 
		goto error;
	}
    }
    else 
	goto error;
    //pics--;
    //}
    free(android_show);

    pthread_mutex_lock(&android_mutex);  
    android_task_ready = 1;  
    pthread_cond_wait(&android_cond,&android_mutex);  
    pthread_mutex_unlock(&android_mutex);  

    close(sockfd);
    return 0;
error:
    error("error");
}

int write_ppm_32(void* d_data,int d_width,int d_height)
{
    FILE* fp;
    uint8_t *p;
    int n;
    char* outf ="ahoo.ppm";

    fp = fopen(outf,"w");
    if (NULL == fp) {
	fprintf(stderr, "snappy: can't open %s\n", outf);
	return -1;
    }
    fprintf(fp, "P6\n%d %d\n255\n",
	    d_width, d_height);
    n = d_width * d_height;
    p = d_data;
    while (n > 0) {
	fputc(p[2], fp);
	fputc(p[1], fp);
	fputc(p[0], fp);
	p += 4;
	n--;
    }
    fclose(fp);
    return 0;
}


void error(const char *msg)
{
    perror(msg);
    exit(0);
}
