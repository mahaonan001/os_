#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <semaphore.h>
#include <sys/prctl.h>
#include <string.h>
#include <signal.h>
#include "thread.h"
#define VERSION 23
#define BUFSIZE 8096
#define ERROR      42
#define LOG        44
#define FORBIDDEN 403
#define NOTFOUND  404
#define NUMTHREAD 200

struct {
  char *ext;
  char *filetype;
} extensions [] = {
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

typedef struct {
  int hit;
  int fd;
  char *buffer;
  int file_fd;
} webparm;

threadpool read_pool;
threadpool send_pool;
threadpool file_pool;
void read_file(void *data);
void send_msg(void *data);
void read_msg(void *data);
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



void logger(int type, char *s1, char *s2, int socket_fd) {
  struct timeval b, e;
  int fd;
  char logbuffer[BUFSIZE*2];
  switch (type) {
    case ERROR: 
      sprintf(logbuffer, "%s ERROR: %s:%s Errno=%d exiting pid=%d", get_time(), s1, s2, errno, getpid());
      break;
    case FORBIDDEN: 
      write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\n The requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n", 271);
      sprintf(logbuffer, "%s FORBIDDEN: %s:%s", get_time(), s1, s2);
      break;
    case NOTFOUND: 
      write(socket_fd, "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n", 224);
      sprintf(logbuffer, "%s NOT FOUND: %s:%s", get_time(), s1, s2);
      break;
    case LOG: 
      sprintf(logbuffer, "%s INFO: %s:%s:%d", get_time(), s1, s2, socket_fd);
      break;
  }

  if ((fd = open("webserver.log", O_CREAT | O_WRONLY | O_APPEND, 0644)) >= 0) {
    write(fd, logbuffer, strlen(logbuffer));
    write(fd, "\n", 1);
    close(fd);
  }
}
void read_msg(void *data) {
  webparm param = *((webparm *)data);
  int fd = param.fd;
  int hit = param.hit;
  char buffer[BUFSIZE + 1];
  long ret = read(fd, buffer, BUFSIZE);

  if (ret == 0 || ret == -1) {
    logger(FORBIDDEN, "failed to read browser request", "", fd);
    close(fd);
    return;
  }

  if (ret > 0 && ret < BUFSIZE)
    buffer[ret] = 0;
  else buffer[0] = 0;

  for (long i = 0; i < ret; i++)
    if (buffer[i] == '\r' || buffer[i] == '\n')
      buffer[i] = '*';

  logger(LOG, "request", buffer, hit);

  if (strncmp(buffer, "GET ", 4) && strncmp(buffer, "get ", 4)) {
    logger(FORBIDDEN, "Only simple GET operation supported", buffer, fd);
    close(fd);
    return;
  }

  for (long i = 4; i < BUFSIZE; i++) {
    if (buffer[i] == ' ') {
      buffer[i] = 0;
      break;
    }
  }

  for (long j = 0; j < ret - 1; j++) {
    if (buffer[j] == '.' && buffer[j + 1] == '.') {
      logger(FORBIDDEN, "Parent directory (..) path names not supported", buffer, fd);
      close(fd);
      return;
    }
  }

  if (!strncmp(&buffer[0], "GET /\0", 6) || !strncmp(&buffer[0], "get /\0", 6))
    strcpy(buffer, "GET /index.html");

  param.buffer = strdup(buffer);
  addTask2ThreadPool(&send_pool, &(task){.function = read_file, .arg = &param});
}

void read_file(void *data) {
  webparm param = *((webparm *)data);
  int fd = param.fd;
  int hit = param.hit;
  char *buffer = param.buffer;

  int buflen = strlen(buffer);
  char *fstr = NULL;
  for (int i = 0; extensions[i].ext != 0; i++) {
    int len = strlen(extensions[i].ext);
    if (!strncmp(&buffer[buflen - len], extensions[i].ext, len)) {
      fstr = extensions[i].filetype;
      break;
    }
  }

  if (fstr == 0) {
    logger(FORBIDDEN, "file extension type not supported", buffer, fd);
    close(fd);
    return;
  }

  param.file_fd = open(&buffer[5], O_RDONLY);
  if (param.file_fd == -1) {
    logger(NOTFOUND, "failed to open file", &buffer[5], fd);
    close(fd);
    return;
  }

  logger(LOG, "SEND", &buffer[5], hit);
  long len = get_file_size(&buffer[5]);
  lseek(param.file_fd, (off_t)0, SEEK_SET);
  sprintf(buffer, "HTTP/1.1 200 OK\nServer: nweb/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, len, fstr);
  logger(LOG, "Header", buffer, hit);

  write(fd, buffer, strlen(buffer));

  addTask2ThreadPool(&file_pool, &(task){.function = send_msg, .arg = &param});
}

void send_msg(void *data) {
  webparm param = *((webparm *)data);
  int fd = param.fd;
  int file_fd = param.file_fd;
  char buffer[BUFSIZE + 1];
  long ret;

  while ((ret = read(file_fd, buffer, BUFSIZE)) > 0) {
    write(fd, buffer, ret);
  }

  usleep(1000);
  close(file_fd);
  close(fd);
}

// performance_data perf_data;
void print_performance_data(int signum) {
    pthread_mutex_lock(&perf_data.lock);
    printf("Average Active Time: %f\n", perf_data.total_active_time / perf_data.total_threads);
    printf("Average Block Time: %f\n", perf_data.total_block_time / perf_data.total_threads);
    printf("Max Active Threads: %d\n", perf_data.max_active_threads);
    printf("Min Active Threads: %d\n", perf_data.min_active_threads);
    printf("Current Active Threads: %d\n", perf_data.active_threads);
    printf("Message Queue Length: %d\n", perf_data.queue_length);
    pthread_mutex_unlock(&perf_data.lock);
}

void setup_timer() {
    struct sigaction sa;
    struct itimerval timer;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &print_performance_data;
    sigaction(SIGALRM, &sa, NULL);

    timer.it_value.tv_sec = 5;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 5;
    timer.it_interval.tv_usec = 0;

    setitimer(ITIMER_REAL, &timer, NULL);
}

int main(int argc, char **argv) {
    int i, port, listenfd, socketfd, hit;
    socklen_t length;
    static struct sockaddr_in cli_addr;
    static struct sockaddr_in serv_addr;

    if (argc < 3 || argc > 3 || !strcmp(argv[1], "-?")) {
        printf("hint: nweb Port-Number Top-Directory\t\tversion %d\n\n"
               "\tnweb is a small and very safe mini web server\n"
               "\tnweb only serves out file/web pages with extensions named below\n"
               "\t and only from the named directory or its sub-directories.\n"
               "\tThere are no fancy features = safe and secure.\n\n"
               "\tExample:webserver 8181 /home/nwebdir &\n\n"
               "\tOnly Supports:", VERSION);

        for (i = 0; extensions[i].ext != 0; i++)
            printf(" %s", extensions[i].ext);

        printf("\n\tNot Supported: URLs including \"..\", Java, Javascript, CGI\n"
               "\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin \n"
               "\tNo warranty given or implied\n\tNigel Griffiths nag@uk.ibm.com\n");
        exit(0);
    }

    if (!strncmp(argv[2], "/", 2) || !strncmp(argv[2], "/etc", 5) ||
        !strncmp(argv[2], "/bin", 5) || !strncmp(argv[2], "/lib", 5) ||
        !strncmp(argv[2], "/tmp", 5) || !strncmp(argv[2], "/usr", 5) ||
        !strncmp(argv[2], "/dev", 5) || !strncmp(argv[2], "/sbin", 6)) {
        printf("ERROR: Bad top directory %s, see nweb -?\n", argv[2]);
        exit(3);
    }

    if (chdir(argv[2]) == -1) {
        printf("ERROR: Can't Change to directory %s\n", argv[2]);
        exit(4);
    }

    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    for (i = 0; i < 30; i++)
        close(i);
    setpgrp();

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        logger(ERROR, "system call", "socket", 0);

    port = atoi(argv[1]);
    if (port < 0 || port > 60000)
        logger(ERROR, "Invalid port number (try 1->60000)", argv[1], 0);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        logger(ERROR, "system call", "bind", 0);
    if (listen(listenfd, 64) < 0)
        logger(ERROR, "system call", "listen", 0);

    // 初始化性能监测数据
    memset(&perf_data, 0, sizeof(performance_data));
    pthread_mutex_init(&perf_data.lock, NULL);

    // 设置定时器
    setup_timer();

    // 初始化三个线程池
    threadpool read_pool = *initThreadPool(NUMTHREAD);
    threadpool send_pool = *initThreadPool(NUMTHREAD);
    threadpool file_pool = *initThreadPool(NUMTHREAD);

    for (hit = 1; ; hit++) {
        length = sizeof(cli_addr);
        if ((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
            logger(ERROR, "system call", "accept", 0);

        webparm *param = (webparm *)malloc(sizeof(webparm));
        param->hit = hit;
        param->fd = socketfd;

        // 将任务添加到 read_pool
        task *curtask = (task *)malloc(sizeof(task));
        curtask->function = read_msg;
        curtask->arg = param;
        addTask2ThreadPool(&read_pool, curtask);
    }

    return 0;
}