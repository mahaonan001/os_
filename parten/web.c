#include"web.h"
#include <sys/stat.h>
#include <string.h>
#include<fcntl.h>
#include<unistd.h>
#include"log.h"
#include "thread.h"
#include <stdlib.h>
extension extensions[] = {
  {"gif", "image/gif" },
  {"jpg", "image/jpg" },
  {"jpeg","image/jpeg"},
  {"png", "image/png" },
  {"ico", "image/ico" },
  {"zip", "image/zip" },
  {"gz",  "image/gz"  },
  {"tar", "image/tar" },
  {"htm", "text/html" },
  {"html","text/html" },
  {0,0} };
void read_msg(void *data) {
    web_request_t *req = (web_request_t *)data;
    char s[50];
    sprintf(s,"this is read func");
    logger(LOG, "read_msg", s, req->fd);
    
    long ret = read(req->fd, req->buffer, BUFSIZE);

    if (ret == 0 || ret == -1) {
        logger(FORBIDDEN, "failed to read browser request", "", req->fd);
        return;
    }

    if (ret > 0 && ret < BUFSIZE)
        req->buffer[ret] = 0;
    else
        req->buffer[0] = 0;

    for (int i = 0; i < ret; i++)
        if (req->buffer[i] == '\r' || req->buffer[i] == '\n')
            req->buffer[i] = '*';

    logger(LOG, "request", req->buffer, req->hit);

    if (strncmp(req->buffer, "GET ", 4) && strncmp(req->buffer, "get ", 4)) {
        logger(FORBIDDEN, "Only simple GET operation supported", req->buffer, req->fd);
        return;
    }

    for (int i = 4; i < BUFSIZE; i++) {
        if (req->buffer[i] == ' ') {
            req->buffer[i] = 0;
            logger(LOG, "request", req->buffer, req->hit);
            break;
        }
    }

    for (int j = 0; j < BUFSIZE - 1; j++)
        if (req->buffer[j] == '.' && req->buffer[j + 1] == '.') {
            logger(FORBIDDEN, "Parent directory (..) path names not supported", req->buffer, req->fd);
            return;
        }

    if (!strncmp(&req->buffer[0], "GET /\0", 6) || !strncmp(&req->buffer[0], "get /\0", 6))
        strcpy(req->buffer, "GET /index.html");

    int buflen = strlen(req->buffer);
    req->fstr = NULL;
    for (int i = 0; extensions[i].ext != 0; i++) {
        int len = strlen(extensions[i].ext);
        if (!strncmp(&req->buffer[buflen - len], extensions[i].ext, len)) {
            req->fstr = extensions[i].filetype;
            logger(LOG, "filetype", req->fstr, req->hit);
            break;
        }
    }

    if (req->fstr == NULL) {
        logger(FORBIDDEN, "file extension type not supported", req->buffer, req->fd);
        return;
    }
    strncpy(req->filepath, &req->buffer[4], buflen - 4);
    // task *curtask = (task *)malloc(sizeof(task));
    // curtask->function = read_file;
    // curtask->arg = malloc(sizeof(web_request_t));
    // *((web_request_t*)curtask->arg) = *req;
    // addTask2ThreadPool(ReadFilePool, curtask);
    read_file(req);
}

void read_file(void *data) {
    web_request_t *req = (web_request_t *)data;
    char s[50];
    sprintf(s,"this is read_file func");
    logger(LOG, req->filepath, s, 0);
    if ((req->file_fd = open(&req->buffer[5], O_RDONLY)) == -1) {
        logger(NOTFOUND, "failed to open file", req->filepath, req->fd);
        return;
    }

    logger(LOG, "SEND", req->filepath, req->hit);
    lseek(req->file_fd, 0, SEEK_SET);
    // task *curtask = (task *)malloc(sizeof(task));
    // curtask->function = read_file;
    // curtask->arg = malloc(sizeof(web_request_t));
    // *((web_request_t*)curtask->arg) = *req;
    // addTask2ThreadPool(ReadFilePool, curtask);
    send_msg(req);
}

void send_msg(void *data) {
    web_request_t *req = (web_request_t *)data;
    sprintf(req->buffer, "HTTP/1.1 200 OK\nServer: nweb/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, req->len, req->fstr);
    logger(LOG, "Header", req->buffer, req->hit);
    write(req->fd, req->buffer, strlen(req->buffer));

    long ret;
    while ((ret = read(req->file_fd, req->buffer, BUFSIZE)) > 0) {
        write(req->fd, req->buffer, ret);
    }
    sleep(1);
    close(req->fd);
}
