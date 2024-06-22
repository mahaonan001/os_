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

// void web(void *data) {
//   int fd;
//   int hit;
//   int j, file_fd, buflen;
//   long i, ret, len;
//   char *fstr;
//   char buffer[BUFSIZE + 1];
//   web_request_t param = *((web_request_t *)data);
//   fd = param.fd;
//   hit = param.hit;
//   ret = read(fd, buffer, BUFSIZE);


//   if (ret == 0 || ret == -1) {
//     logger(FORBIDDEN, "failed to read browser request", "", fd);
//   } else {
//     if (ret > 0 && ret < BUFSIZE)
//       buffer[ret] = 0;
//     else buffer[0] = 0;
//     for (i = 0; i < ret; i++)
//       if (buffer[i] == '\r' || buffer[i] == '\n')
//         buffer[i] = '*';
//     logger(LOG, "request", buffer, hit);

//     if (strncmp(buffer, "GET ", 4) && strncmp(buffer, "get ", 4)) {
//       logger(FORBIDDEN, "Only simple GET operation supported", buffer, fd);
//     }
//     for (i = 4; i < BUFSIZE; i++) {
//       if (buffer[i] == ' ') {
//         buffer[i] = 0;
//         break;
//       }
//     }
//     for (j = 0; j < i - 1; j++)
//       if (buffer[j] == '.' && buffer[j + 1] == '.') {
//         logger(FORBIDDEN, "Parent directory (..) path names not supported", buffer, fd);
//       }
//     if (!strncmp(&buffer[0], "GET /\0", 6) || !strncmp(&buffer[0], "get /\0", 6))
//       strcpy(buffer, "GET /index.html");

//     buflen = strlen(buffer);
//     fstr = (char *)0;
//     for (i = 0; extensions[i].ext != 0; i++) {
//       len = strlen(extensions[i].ext);
//       if (!strncmp(&buffer[buflen - len], extensions[i].ext, len)) {
//         fstr = extensions[i].filetype;
//         break;
//       }
//     }
//     if (fstr == 0) logger(FORBIDDEN, "file extension type not supported", buffer, fd);

//     if ((file_fd = open(&buffer[5], O_RDONLY)) == -1) {
//       logger(NOTFOUND, "failed to open file", &buffer[5], fd);
//     }
//     logger(LOG, "SEND", &buffer[5], hit);
//     len = get_file_size(&buffer[5]);
//     lseek(file_fd, (off_t)0, SEEK_SET);
//     sprintf(buffer, "HTTP/1.1 200 OK\nServer: nweb/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, len, fstr);
//     logger(LOG, "Header", buffer, hit);

//     write(fd, buffer, strlen(buffer));


//     while ((ret = read(file_fd, buffer, BUFSIZE)) > 0) {
//       write(fd, buffer, ret);

//     }
//     usleep(1000);
//     close(file_fd);
//   }
//   close(fd);
// }

void read_msg(void *data) {
    shared_data_t *shared = (shared_data_t *)data;
    long ret = read(shared->fd, shared->buffer, BUFSIZE);
    
    if (ret == 0 || ret == -1) {
        logger(FORBIDDEN, "failed to read browser request", "", shared->fd);
        close(shared->fd);
        return;
    }

    if (ret > 0 && ret < BUFSIZE)
        shared->buffer[ret] = 0;
    else
        shared->buffer[0] = 0;

    for (long i = 0; i < ret; i++)
        if (shared->buffer[i] == '\r' || shared->buffer[i] == '\n')
            shared->buffer[i] = '*';
    logger(LOG, "request", shared->buffer, shared->hit);

    pthread_mutex_lock(&shared->mutex);
    shared->buffer_len = ret;
    shared->read_done = 1;
    pthread_cond_signal(&shared->cond);
    pthread_mutex_unlock(&shared->mutex);
}

void send_msg(void *data) {
    shared_data_t *shared = (shared_data_t *)data;

    pthread_mutex_lock(&shared->mutex);
    while (!shared->read_done)
        pthread_cond_wait(&shared->cond, &shared->mutex);

    char *buffer = shared->buffer;
    int fd = shared->fd;
    int hit = shared->hit;
    long i;

    if (strncmp(buffer, "GET ", 4) && strncmp(buffer, "get ", 4)) {
        logger(FORBIDDEN, "Only simple GET operation supported", buffer, fd);
        pthread_mutex_unlock(&shared->mutex);
        close(fd);
        return;
    }

    for (i = 4; i < BUFSIZE; i++) {
        if (buffer[i] == ' ') {
            buffer[i] = 0;
            break;
        }
    }

    for (int j = 0; j < i - 1; j++) {
        if (buffer[j] == '.' && buffer[j + 1] == '.') {
            logger(FORBIDDEN, "Parent directory (..) path names not supported", buffer, fd);
            pthread_mutex_unlock(&shared->mutex);
            close(fd);
            return;
        }
    }

    if (!strncmp(&buffer[0], "GET /\0", 6) || !strncmp(&buffer[0], "get /\0", 6))
        strcpy(buffer, "GET /index.html");

    shared->fstr = NULL;
    int buflen = strlen(buffer);
    for (i = 0; extensions[i].ext != 0; i++) {
        int len = strlen(extensions[i].ext);
        if (!strncmp(&buffer[buflen - len], extensions[i].ext, len)) {
            shared->fstr = extensions[i].filetype;
            break;
        }
    }
    if (shared->fstr == 0) {
        logger(FORBIDDEN, "file extension type not supported", buffer, fd);
        pthread_mutex_unlock(&shared->mutex);
        close(fd);
        return;
    }

    if ((shared->file_fd = open(&buffer[5], O_RDONLY)) == -1) {
        logger(NOTFOUND, "failed to open file", &buffer[5], fd);
        pthread_mutex_unlock(&shared->mutex);
        close(fd);
        return;
    }

    shared->file_size = get_file_size(&buffer[5]);
    lseek(shared->file_fd, (off_t)0, SEEK_SET);

    sprintf(buffer, "HTTP/1.1 200 OK\nServer: nweb/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, shared->file_size, shared->fstr);
    logger(LOG, "Header", buffer, hit);

    write(fd, buffer, strlen(buffer));

    shared->header_sent = 1;
    pthread_cond_signal(&shared->cond);
    pthread_mutex_unlock(&shared->mutex);
}
void read_file(void *data) {
    shared_data_t *shared = (shared_data_t *)data;

    pthread_mutex_lock(&shared->mutex);
    while (!shared->header_sent)
        pthread_cond_wait(&shared->cond, &shared->mutex);
    pthread_mutex_unlock(&shared->mutex);

    char buffer[BUFSIZE];
    long ret;

    while ((ret = read(shared->file_fd, buffer, BUFSIZE)) > 0) {
        write(shared->fd, buffer, ret);
    }

    usleep(1000);
    close(shared->file_fd);
    close(shared->fd);
}

