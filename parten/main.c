#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include "web.h"
threadpool *pool = NULL;
threadpool *ReadFilePool = NULL;
threadpool *SendMsgPool = NULL;

void web(void *data) {
    web_request_t param = *((web_request_t *)data);
    shared_data_t *shared = malloc(sizeof(shared_data_t));
    shared->fd = param.fd;
    shared->hit = param.hit;
    shared->read_done = 0;
    shared->header_sent = 0;
    pthread_mutex_init(&shared->mutex, NULL);
    pthread_cond_init(&shared->cond, NULL);

    task *task_read_msg = malloc(sizeof(task));
    task_read_msg->function = read_msg;
    task_read_msg->arg = shared;

    task *task_send_msg = malloc(sizeof(task));
    task_send_msg->function = send_msg;
    task_send_msg->arg = shared;

    task *task_read_file = malloc(sizeof(task));
    task_read_file->function = read_file;
    task_read_file->arg = shared;

    addTask2ThreadPool(pool, task_read_msg);
    addTask2ThreadPool(ReadFilePool, task_send_msg);
    addTask2ThreadPool(SendMsgPool, task_read_file);
}

int main(int argc, char **argv) {
  pool = initThreadPool(NUMTHREAD); // 修改为指针
  ReadFilePool = initThreadPool(NUMTHREAD);
  SendMsgPool = initThreadPool(NUMTHREAD);
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

  for (hit = 1; ; hit++) {
    length = sizeof(cli_addr);
    if ((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
      logger(ERROR, "system call", "accept", 0);

    web_request_t *param=malloc(sizeof(web_request_t));
    param->hit = hit;
    param->fd = socketfd;
    shared_data_t *shared = malloc(sizeof(shared_data_t));
    shared->fd = param->fd;
    shared->hit = param->hit;
    shared->read_done = 0;
    shared->header_sent = 0;
    pthread_mutex_init(&shared->mutex, NULL);
    pthread_cond_init(&shared->cond, NULL);

    task *task_read_msg = malloc(sizeof(task));
    task_read_msg->function = read_msg;
    task_read_msg->arg = shared;

    task *task_send_msg = malloc(sizeof(task));
    task_send_msg->function = send_msg;
    task_send_msg->arg = shared;

    task *task_read_file = malloc(sizeof(task));
    task_read_file->function = read_file;
    task_read_file->arg = shared;

    addTask2ThreadPool(pool, task_read_msg);
    addTask2ThreadPool(pool, task_send_msg);
    addTask2ThreadPool(pool, task_read_file);
  }
}
