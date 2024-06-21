#ifndef WEB_H
#define WEB_H
#include<stdio.h>
#include<pthread.h>
#include"log.h"
#include"thread.h"
#define VERSION 23
#define BUFSIZE 8096
#define ERROR      42
#define LOG        44
#define FORBIDDEN 403
#define NOTFOUND  404
#define NUMTHREAD 200
void read_file(void *data);
void send_msg(void *data);
void read_msg(void *data);
void web(void *data);
typedef struct {
    int fd;
    int hit;
    int file_fd;
    char buffer[BUFSIZE + 1];
    char filepath[256];
    long len;
    char *fstr;
    pthread_mutex_t *lock;
} web_request_t;

typedef struct {
  char *ext;
  char *filetype;
} extension;

extern extension extensions[];

extern threadpool *ReadFilePool;
extern threadpool *SendMsgPool;
#endif // WEB_H