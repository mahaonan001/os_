#include"log.h"
#include"web.h"
#include"thread.h"
#include<errno.h>
#include<time.h>
#include<fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
extern char *get_time() {
    time_t time_now;
    time(&time_now);
    char *time_str = ctime(&time_now);
    if (time_str) {
        time_str[strlen(time_str) - 1] = '\0'; // 去掉换行符
    }
    return time_str;
}

void logger(int type, char *s1, char *s2, int socket_fd) {
    int fd;
    char logbuffer[BUFSIZE*2];
    char *time_str = get_time();
    if (time_str == NULL) {
        time_str = "unknown_time";
    }

    switch (type) {
        case ERROR:
            snprintf(logbuffer, sizeof(logbuffer), "%s ERROR: %s:%s Errno=%d exiting pid=%d", time_str, s1, s2, errno, getpid());
            break;
        case FORBIDDEN:
            write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\n The requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n", 271);
            snprintf(logbuffer, sizeof(logbuffer), "%s FORBIDDEN: %s:%s", time_str, s1, s2);
            break;
        case NOTFOUND:
            write(socket_fd, "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n", 224);
            snprintf(logbuffer, sizeof(logbuffer), "%s NOT FOUND: %s:%s", time_str, s1, s2);
            break;
        case LOG:
            snprintf(logbuffer, sizeof(logbuffer), "%s INFO: %s:%s:%d", time_str, s1, s2, socket_fd);
            break;
    }

    if ((fd = open("webserver.log", O_CREAT | O_WRONLY | O_APPEND, 0644)) >= 0) {
        write(fd, logbuffer, strlen(logbuffer));
        write(fd, "\n", 1);
        close(fd);
    }
}

unsigned long get_file_size(char *path) {
    unsigned long size = -1;
    struct stat statbuff;
    if (stat(path, &statbuff) < 0) {
        return size;
    } else {
        size = statbuff.st_size;
    }
    return size;
}