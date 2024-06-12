#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <semaphore.h>
#include <stdbool.h>
#include <sys/prctl.h>

#define VERSION 23
#define BUFSIZE 8096
#define ERROR      42
#define LOG        44
#define FORBIDDEN 403
#define NOTFOUND  404

typedef struct staconv {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool status;
} staconv;

typedef struct task {
    struct task *next;
    void (*function)(void *arg);
    void *arg;
} task;

typedef struct taskqueue {
    pthread_mutex_t mutex;
    task *front;
    task *end;
    staconv *has_jobs;
    int len;
} taskqueue;

typedef struct thread {
    int id;
    pthread_t pthread;
    struct threadpool *pool;
} thread;

typedef struct threadpool {
    thread **threads;
    volatile int num_threads;
    volatile int num_working;
    pthread_mutex_t thcont_lock;
    pthread_cond_t threads_all_idle;
    taskqueue queue;
    volatile bool is_alive;
} threadpool;

void init_taskqueue(taskqueue *queue);
struct threadpool *initThreadPool(int num_threads);
void push_taskqueue(taskqueue *queue, task *curtask);
void addTask2ThreadPool(threadpool *pool, task *curtask);
void waitThreadPool(threadpool *pool);
void destroyThreadPool(threadpool *pool);
int getNumofThreadWorking(threadpool *pool);
int create_thread(struct threadpool *pool, struct thread **pthread, int id);
void destroy_taskqueue(taskqueue *queue);
task* take_taskqueue(taskqueue *queue);
void *thread_do(void *arg);

void init_taskqueue(taskqueue *queue) {
    queue->len = 0;
    queue->front = NULL;
    queue->end = NULL;
    queue->has_jobs = (staconv *)malloc(sizeof(staconv));
    pthread_mutex_init(&(queue->mutex), NULL);
    pthread_mutex_init(&(queue->has_jobs->mutex), NULL);
    pthread_cond_init(&(queue->has_jobs->cond), NULL);
    queue->has_jobs->status = false;
}

void destroy_taskqueue(taskqueue *queue) {
    while (queue->len) {
        task *t = take_taskqueue(queue);
        if (t) free(t);
    }
    pthread_mutex_destroy(&(queue->mutex));
    pthread_mutex_destroy(&(queue->has_jobs->mutex));
    pthread_cond_destroy(&(queue->has_jobs->cond));
    free(queue->has_jobs);
}

void push_taskqueue(taskqueue *queue, task *curtask) {
    pthread_mutex_lock(&(queue->mutex));
    curtask->next = NULL;
    if (queue->len == 0) {
        queue->front = curtask;
        queue->end = curtask;
    } else {
        queue->end->next = curtask;
        queue->end = curtask;
    }
    queue->len++;
    queue->has_jobs->status = true;
    pthread_cond_signal(&(queue->has_jobs->cond));
    pthread_mutex_unlock(&(queue->mutex));
}

task *take_taskqueue(taskqueue *queue) {
    pthread_mutex_lock(&(queue->mutex));
    task *curtask = queue->front;
    if (queue->len > 0) {
        queue->front = curtask->next;
        queue->len--;
        if (queue->len == 0) {
            queue->end = NULL;
            queue->has_jobs->status = false;
        }
    }
    pthread_mutex_unlock(&(queue->mutex));
    return curtask;
}

void *thread_do(void *arg) {
    thread *thr = (thread *)arg;
    threadpool *pool = thr->pool;

    while (pool->is_alive) {
        pthread_mutex_lock(&(pool->queue.has_jobs->mutex));
        while (pool->queue.has_jobs->status == false && pool->is_alive) {
            pthread_cond_wait(&(pool->queue.has_jobs->cond), &(pool->queue.has_jobs->mutex));
        }
        pthread_mutex_unlock(&(pool->queue.has_jobs->mutex));

        if (pool->is_alive) {
            pthread_mutex_lock(&(pool->thcont_lock));
            pool->num_working++;
            pthread_mutex_unlock(&(pool->thcont_lock));

            task *curtask = take_taskqueue(&(pool->queue));
            if (curtask) {
                curtask->function(curtask->arg);
                free(curtask->arg);  // 释放任务参数
                free(curtask);  // 释放任务
            }

            pthread_mutex_lock(&(pool->thcont_lock));
            pool->num_working--;
            if (pool->num_working == 0) {
                pthread_cond_signal(&(pool->threads_all_idle));
            }
            pthread_mutex_unlock(&(pool->thcont_lock));
        }
    }
    pthread_mutex_lock(&(pool->thcont_lock));
    pool->num_threads--;
    pthread_mutex_unlock(&(pool->thcont_lock));
    return NULL;
}

int create_thread(threadpool *pool, thread **pthread, int id) {
    *pthread = (thread *)malloc(sizeof(thread));
    if (*pthread == NULL) {
        return -1;
    }
    (*pthread)->pool = pool;
    (*pthread)->id = id;
    pthread_create(&((*pthread)->pthread), NULL, thread_do, (*pthread));
    pthread_detach((*pthread)->pthread);
    return 0;
}

struct threadpool *initThreadPool(int num_threads) {
    threadpool *pool = (threadpool *)malloc(sizeof(threadpool));
    pool->num_threads = 0;
    pool->num_working = 0;
    pool->is_alive = true;
    pthread_mutex_init(&(pool->thcont_lock), NULL);
    pthread_cond_init(&(pool->threads_all_idle), NULL);
    init_taskqueue(&(pool->queue));
    pool->threads = (thread **)malloc(num_threads * sizeof(thread *));
    for (int i = 0; i < num_threads; i++) {
        create_thread(pool, &(pool->threads[i]), i);
        pool->num_threads++;
    }
    return pool;
}

void addTask2ThreadPool(threadpool *pool, task *curtask) {
    push_taskqueue(&(pool->queue), curtask);
}

void waitThreadPool(threadpool *pool) {
    pthread_mutex_lock(&(pool->thcont_lock));
    while (pool->queue.len || pool->num_working) {
        pthread_cond_wait(&(pool->threads_all_idle), &(pool->thcont_lock));
    }
    pthread_mutex_unlock(&(pool->thcont_lock));
}

void destroyThreadPool(threadpool *pool) {
    pool->is_alive = false;
    pthread_mutex_lock(&(pool->queue.has_jobs->mutex));
    pthread_cond_broadcast(&(pool->queue.has_jobs->cond));
    pthread_mutex_unlock(&(pool->queue.has_jobs->mutex));

    for (int i = 0; i < pool->num_threads; i++) {
        free(pool->threads[i]);
    }
    free(pool->threads);
    destroy_taskqueue(&(pool->queue));
    pthread_mutex_destroy(&(pool->thcont_lock));
    pthread_cond_destroy(&(pool->threads_all_idle));
    free(pool);
}

int getNumofThreadWorking(threadpool *pool) {
    pthread_mutex_lock(&(pool->thcont_lock));
    int num = pool->num_working;
    pthread_mutex_unlock(&(pool->thcont_lock));
    return num;
}

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

typedef struct{
  int hit;
  int fd;
} webparm;

double rs = 0, ws = 0, rd = 0, wd = 0;
pthread_rwlock_t rwlock0 = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t rwlock1 = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t rwlock2 = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t rwlock3 = PTHREAD_RWLOCK_INITIALIZER;

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

char *get_time() {
  time_t time_now;
  time(&time_now);
  return ctime(&time_now);
}

void logger(int type, char *s1, char *s2, int socket_fd) {
  struct timeval b, e;
  gettimeofday(&b, NULL);
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
  gettimeofday(&e, NULL);
  pthread_rwlock_wrlock(&rwlock3);
  wd += (double)((e.tv_sec - b.tv_sec) * 1000.0 + (e.tv_usec - b.tv_usec) / 1000.0);
  pthread_rwlock_unlock(&rwlock3);
}

void web(void *data) {
  int fd;
  int hit;
  int j, file_fd, buflen;
  long i, ret, len;
  char *fstr;
  char buffer[BUFSIZE + 1];
  struct timeval b, e;
  webparm param = *((webparm *)data);
  fd = param.fd;
  hit = param.hit;
  gettimeofday(&b, NULL);
  ret = read(fd, buffer, BUFSIZE);
  gettimeofday(&e, NULL);
  pthread_rwlock_wrlock(&rwlock0);
  rs += (double)((e.tv_sec - b.tv_sec) * 1000.0 + (e.tv_usec - b.tv_usec) / 1000.0);
  pthread_rwlock_unlock(&rwlock0);

  if (ret == 0 || ret == -1) {
    logger(FORBIDDEN, "failed to read browser request", "", fd);
  } else {
    if (ret > 0 && ret < BUFSIZE)
      buffer[ret] = 0;
    else buffer[0] = 0;
    for (i = 0; i < ret; i++)
      if (buffer[i] == '\r' || buffer[i] == '\n')
        buffer[i] = '*';
    logger(LOG, "request", buffer, hit);

    if (strncmp(buffer, "GET ", 4) && strncmp(buffer, "get ", 4)) {
      logger(FORBIDDEN, "Only simple GET operation supported", buffer, fd);
    }
    for (i = 4; i < BUFSIZE; i++) {
      if (buffer[i] == ' ') {
        buffer[i] = 0;
        break;
      }
    }
    for (j = 0; j < i - 1; j++)
      if (buffer[j] == '.' && buffer[j + 1] == '.') {
        logger(FORBIDDEN, "Parent directory (..) path names not supported", buffer, fd);
      }
    if (!strncmp(&buffer[0], "GET /\0", 6) || !strncmp(&buffer[0], "get /\0", 6))
      strcpy(buffer, "GET /index.html");

    buflen = strlen(buffer);
    fstr = (char *)0;
    for (i = 0; extensions[i].ext != 0; i++) {
      len = strlen(extensions[i].ext);
      if (!strncmp(&buffer[buflen - len], extensions[i].ext, len)) {
        fstr = extensions[i].filetype;
        break;
      }
    }
    if (fstr == 0) logger(FORBIDDEN, "file extension type not supported", buffer, fd);

    if ((file_fd = open(&buffer[5], O_RDONLY)) == -1) {
      logger(NOTFOUND, "failed to open file", &buffer[5], fd);
    }
    logger(LOG, "SEND", &buffer[5], hit);
    len = get_file_size(&buffer[5]);
    lseek(file_fd, (off_t)0, SEEK_SET);
    sprintf(buffer, "HTTP/1.1 200 OK\nServer: nweb/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, len, fstr);
    logger(LOG, "Header", buffer, hit);

    gettimeofday(&b, NULL);
    write(fd, buffer, strlen(buffer));
    gettimeofday(&e, NULL);
    pthread_rwlock_wrlock(&rwlock2);
    rd += (double)((e.tv_sec - b.tv_sec) * 1000.0 + (e.tv_usec - b.tv_usec) / 1000.0);
    pthread_rwlock_unlock(&rwlock2);

    while ((ret = read(file_fd, buffer, BUFSIZE)) > 0) {
      gettimeofday(&b, NULL);
      write(fd, buffer, ret);
      gettimeofday(&e, NULL);
      pthread_rwlock_wrlock(&rwlock1);
      ws += (double)((e.tv_sec - b.tv_sec) * 1000.0 + (e.tv_usec - b.tv_usec) / 1000.0);
      pthread_rwlock_unlock(&rwlock1);
    }
    usleep(1000);
    close(file_fd);
  }
  close(fd);
}

int main(int argc, char **argv) {
  threadpool *pool = initThreadPool(8); // 修改为指针
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
  if (chdir(argv[2]) ==    -1) {
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

  for (hit = 1; ; hit++) {
    length = sizeof(cli_addr);
    if ((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
      logger(ERROR, "system call", "accept", 0);

    webparm param;
    param.hit = hit;
    param.fd = socketfd;
    task *curtask = (task *)malloc(sizeof(task));
    curtask->function = web;
    curtask->arg = malloc(sizeof(param));
    *((webparm*)curtask->arg) = param;
    addTask2ThreadPool(pool, curtask);

    if (hit == 100) {
      char s[10000];
      pthread_rwlock_wrlock(&rwlock0);
      pthread_rwlock_wrlock(&rwlock1);
      pthread_rwlock_wrlock(&rwlock2);
      pthread_rwlock_wrlock(&rwlock3);
      sprintf(s, "共有100000ms完成100个客户端请求，其中\n    平均每个客户端请求处理时间为%lfms\n    平均每个客户端读socket时间为%lfms\n    平均每个客户端写socket时间为%lfms\n    平均每个客户端读网页数据时间为%lfms\n    平均每个客户端写日志文件时间为%lfms\n", (rs + ws + rd + wd) , rs , ws , rd , wd );
      logger(LOG, "time", s, 0);
      pthread_rwlock_unlock(&rwlock3);
      pthread_rwlock_unlock(&rwlock2);
      pthread_rwlock_unlock(&rwlock1);
      pthread_rwlock_unlock(&rwlock0);
    }
  }
  waitThreadPool(pool);
  // 销毁线程池
  destroyThreadPool(pool);
  return 0;
}