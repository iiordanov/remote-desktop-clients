/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2011  Keqisoft,Co,Ltd,Shanghai,China
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
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/un.h>
#include <stdio.h>
#include "spice-common.h"
#include "android-spice.h"
#include "android-spice-priv.h"
#include "jpeg_encoder.h"

extern GMainLoop* volatile android_mainloop;
extern JpegEncoder* volatile android_jpeg_encoder;
extern pthread_mutex_t android_mutex;  
extern pthread_cond_t android_cond;  
extern volatile int android_task_ready;
extern volatile int android_task;
volatile AndroidShow android_show_display;
gboolean key_event(AndroidEventKey* key);

void getval(void* dest,void* src,int type)
{
    int i;
    switch(type)
    {
	case INT:
	    memset(dest,0,4);
	    for(i=0;i<4;i++)
		memcpy(dest+i,src+3-i,1);
	    break;
	case UINT_8:
	    break;
    }
}

void error(const char *msg)
{
//  SPICE_DEBUG(msg);
}

void android_send_task(int task)
{
    while(!android_task_ready);  
    SPICE_DEBUG("send task:%d\n",task);
    pthread_mutex_lock(&android_mutex);  
    android_task_ready = 0;  
    android_task = task;  
    pthread_cond_signal(&android_cond);  
    pthread_mutex_unlock(&android_mutex);  
    while(android_task);  
    SPICE_DEBUG("send task done:%d\n",task);
}

int write_data(int sockfd,uint8_t* buf,int size,int type)
{
    int i;
    int num=0;
    int steps = 0;
    switch(type)
    {
	case INT:
	    while(steps<size)
	    {
		for(i=3;i>=0;i--)
		    num+=write(sockfd,buf+steps+i,1);
			steps+=4;
	    }
    }
    return num;
}

int msg_send_handle(int sockfd)
{
    int n;
    uint8_t* buf = (uint8_t*)&(android_show_display.type);
    n = write_data(sockfd,buf,24,INT);
    if(n<=0)
	goto error;
    n = write(sockfd,android_show_display.data,android_show_display.size);
    if(n<=0)
	goto error;
    free(android_show_display.data);
    SPICE_DEBUG("Image bytes sent:%d",n);
    return 0;
error:
    if(n==0)
	error("connection error!\n");
    else if(n<0)
	error("msg_send error!\n");
    return -1;
}

int raw2jpg(uint8_t* data, int width,int height )
{
    if(android_jpeg_encoder)
	return jpeg_encode(android_jpeg_encoder,60,width,height,data,width*4,&android_show_display.data);
    else
    {
	SPICE_DEBUG("no android_jpeg_encoder found!");
	return 0;
    }
}


/*
 * All the sins comes from the architect of Android: All UI must be
 * written in Java(at least <2.3), so I have to send the image buffer data to Java
 * with Little and Fast flow as possible for the Easy display of latter,hence the use of JPEG_COMP.
 * FIXME:This maybe the only rational way,but that means androidSpice will never leave
 * the status quo labelled EXPERIMENTAL.Tragic...
 *
 * FIXME: androidSpice UI in JAVA can only process the image of horizontal bars
 * So,even QXL gives me normal tiny rectangles,I should send bars to JAVA.
 * So I abandon some little rects to low the flow,this is stupid though...
 */
void android_show(spice_display* d,gint x,gint y,gint w,gint h)
{
    android_show_display.type = ANDROID_SHOW;
    android_show_display.width = d->width; //w;
    android_show_display.height =  h;
    android_show_display.x = 0;//x;
    android_show_display.y = y;
    android_show_display.size =  raw2jpg((uint8_t*)d->data+y*d->width*4,d->width,h);
    SPICE_DEBUG("ANDROID_SHOW for %p:w--%d:h--%d:x--%d:y--%d:jpeg_size--%d",
	    (char*)android_show_display.data,
	    android_show_display.width,
	    android_show_display.height,
	    android_show_display.x,
	    android_show_display.y,
	    android_show_display.size);
    android_send_task(ANDROID_TASK_SHOW);
}

int android_spice_output()
{
    int sockfd, newsockfd, servlen;
    socklen_t clilen;
    struct sockaddr_un  cli_addr, serv_addr;

    if ((sockfd = socket(AF_UNIX,SOCK_STREAM,0)) < 0)
	error("creating socket");
    memset((char *) &serv_addr,0, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    char* sock = "/data/data/com.iiordanov.aSPICE/aspice-output-socket.socket";
    remove(sock);
    strcpy(serv_addr.sun_path, sock);
    servlen=strlen(serv_addr.sun_path) + 
	sizeof(serv_addr.sun_family);
    if(bind(sockfd,(struct sockaddr *)&serv_addr,servlen)<0)
	error("binding socket"); 

    listen(sockfd,5);
    clilen = sizeof(cli_addr);
    newsockfd = accept( sockfd,(struct sockaddr *)&cli_addr,&clilen);
    if (newsockfd < 0) 
	error("accepting");
    while(maintainConnection)
    {  
	pthread_mutex_lock(&android_mutex);  
	android_task_ready = 1;  
	pthread_cond_wait(&android_cond,&android_mutex);  
	SPICE_DEBUG("got task:%d\n",android_task);
	if(android_task == ANDROID_TASK_SHOW) {
	    if(msg_send_handle(newsockfd)) {
	    	maintainConnection = false;
	    }
	    android_task = ANDROID_TASK_IDLE;  
	} else if(android_task == ANDROID_TASK_OVER) {
		maintainConnection = false;
	    android_task = ANDROID_TASK_IDLE;  
	}
	pthread_mutex_unlock(&android_mutex);  
	SPICE_DEBUG("got task done:%d\n",android_task);
    }  

    close(newsockfd);
    close(sockfd);
    SPICE_DEBUG("android output over\n");
    return 0;
}

