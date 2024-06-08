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
#include<time.h>
#include<pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <asm-generic/mman-common.h>
#include <linux/mman.h>
#include <sys/mman.h>
#define VERSION 23 
#define BUFSIZE 8096 
#define ERROR      42 
#define LOG        44 
#define FORBIDDEN 403 
#define NOTFOUND  404 
 
#ifndef SIGCLD 
#   define SIGCLD SIGCHLD 
#endif 



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
    //获取当前时间的函数，返回字符串类型
    char * get_time(){
        time_t time_now;
        time(&time_now);
        return ctime (&time_now);
    }
/* 日志函数，将运行过程中的提示信息记录到webserver.log 文件中*/ 
void logger(int type, char *s1, char *s2, int socket_fd) 
{ 
  int fd ; 
  char logbuffer[BUFSIZE*2]; 
    switch (type) { 
  case ERROR: (void)sprintf(logbuffer,"%s ERROR: %s:%s Errno=%d exiting pid=%d",get_time(),s1, s2, errno,getpid()); 
    break; 
  case FORBIDDEN: 
    (void)write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\n The requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n",271); 
    (void)sprintf(logbuffer,"%s FORBIDDEN: %s:%s",get_time(),s1, s2); 
    break; 
  case NOTFOUND: 
    (void)write(socket_fd, "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n",224); 
    (void)sprintf(logbuffer,"%s NOT FOUND: %s:%s",get_time(),s1, s2); 
    break; 
  case LOG: (void)sprintf(logbuffer,"%s INFO: %s:%s:%d",get_time(),s1, s2,socket_fd); break; 
  } 
  /* 将logbuffer缓存中的消息存入webserver.log文件*/ 
  if((fd = open("webserver.log", O_CREAT| O_WRONLY | O_APPEND,0644)) >= 0) { 
    (void)write(fd,logbuffer,strlen(logbuffer)); 
    (void)write(fd,"\n",1); 
    (void)close(fd); 
  } 
} 
 
/* 此函数完成了WebServer主要功能，它首先解析客户端发送的消息，然后从中获取客户端请求的文
件名，然后根据文件名从本地将此文件读入缓存，并生成相应的HTTP响应消息；最后通过服务器与客户
端的socket通道向客户端返回HTTP响应消息*/ 
void timming(char * prc,time_t begin_,char *arm_s,int socketfd){
  time_t final_ = clock();
  double time_used = ((double)(final_-begin_)/CLOCKS_PER_SEC);
  sprintf(arm_s,"function using %lf s",time_used);
  logger(LOG,prc,arm_s,socketfd);
}
void web(int fd, int hit) 
{ 
  
  int j, file_fd, buflen; 
  long i, ret, len; 
  char * fstr; 
  static char buffer[BUFSIZE+1]; /* 设置静态缓冲区 */ 


  time_t begin_,final_;
  char s_[50];
  begin_ = clock();
  ret =read(fd,buffer,BUFSIZE);   /* 从连接通道中读取客户端的请求消息 */ 
  ////////////////////
  timming("从连接通道中读取客户端的请求消息",begin_,s_,fd);


  if(ret == 0 || ret == -1) {  //如果读取客户端消息失败，则向客户端发送HTTP失败响应信息 
    logger(FORBIDDEN,"failed to read browser request","",fd); 
  } 
  if(ret > 0 && ret < BUFSIZE)  /* 设置有效字符串，即将字符串尾部表示为0 */ 
    buffer[ret]=0;    
  else buffer[0]=0; 
  for(i=0;i<ret;i++)  /* 移除消息字符串中的“CF”和“LF”字符*/ 
    if(buffer[i] == '\r' || buffer[i] == '\n') 
      buffer[i]='*'; 
  logger(LOG,"request",buffer,hit); 
 /*判断客户端HTTP请求消息是否为GET类型，如果不是则给出相应的响应消息*/ 
  if( strncmp(buffer,"GET ",4) && strncmp(buffer,"get ",4) ) { 
    logger(FORBIDDEN,"Only simple GET operation supported",buffer,fd); 
  } 
  for(i=4;i<BUFSIZE;i++) { /* null terminate after the second space to ignore extra stuff */ 
    if(buffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */ 
      buffer[i] = 0; 
      break; 
    } 
  } 

  begin_ = clock();
  for(j=0;j<i-1;j++)   /* 在消息中检测路径，不允许路径中出现“.” */ 
    if(buffer[j] == '.' && buffer[j+1] == '.') { 
      logger(FORBIDDEN,"Parent directory (..) path names not supported",buffer,fd); 
    } 
  if( !strncmp(&buffer[0],"GET /\0",6) || !strncmp(&buffer[0],"get /\0",6) ) 
   /* 如果请求消息中没有包含有效的文件名，则使用默认的文件名index.html */ 
    (void)strcpy(buffer,"GET /index.html"); 
  ///////////////////
  timming("路径检测",begin_,s_,fd);
  /* 根据预定义在extensions中的文件类型，检查请求的文件类型是否本服务器支持 */ 

  begin_ = clock();
  buflen=strlen(buffer); 
  fstr = (char *)0; 
  for(i=0;extensions[i].ext != 0;i++) { 
    len = strlen(extensions[i].ext); 
    if( !strncmp(&buffer[buflen-len], extensions[i].ext, len)) { 
      fstr =extensions[i].filetype; 
      break; 
    } 
  } 
  if(fstr == 0) logger(FORBIDDEN,"file extension type not supported",buffer,fd); 
  timming("检测文件类型",begin_,s_,fd);

  begin_=clock();
  if(( file_fd = open(&buffer[5],O_RDONLY)) == -1) {  /* 打开指定的文件名*/ 
    logger(NOTFOUND, "failed to open file",&buffer[5],fd); 
  } 
  logger(LOG,"SEND",&buffer[5],hit); 
  len = (long)lseek(file_fd, (off_t)0, SEEK_END); /* 通过lseek 获取文件长度*/ 
   (void)lseek(file_fd, (off_t)0, SEEK_SET); /* 将文件指针移到文件首位置*/ 
  (void)sprintf(buffer,"HTTP/1.1 200 OK\nServer: nweb/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, len, fstr); /* Header + a blank line */ 
  logger(LOG,"Header",buffer,hit); 
  (void)write(fd,buffer,strlen(buffer)); 
 
  /* 不停地从文件里读取文件内容，并通过socket通道向客户端返回文件内容*/ 
  while (  (ret = read(file_fd, buffer, BUFSIZE)) > 0 ) { 
    (void)write(fd,buffer,ret); 
  } 
  sleep(1);  /* sleep的作用是防止消息未发出，已经将此socket通道关闭*/ 
  timming("文件读取发送",begin_,s_,fd);
  close(fd); 
} 
 
int main(int argc, char **argv) 
{ 
  
    double *total_time = mmap(NULL, sizeof(double), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS , -1, 0);//定义double类型的共享内存用于记录全部子程序的总用时
    if (total_time == MAP_FAILED) {//开辟是空间失败
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    
    // 初始化共享的总时间
    *total_time = 0.0;

    // 创建一个信号量用于互斥访问
    sem_t *mutex = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS , -1, 0);
    if (mutex == MAP_FAILED) {//创建信号量失败
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    sem_init(mutex, 0, 1);//初始化初始值为1

  int i, port, listenfd, socketfd, hit;
  socklen_t length; 
  static struct sockaddr_in cli_addr; /* static = initialised to zeros */ 
  static struct sockaddr_in serv_addr; /* static = initialised to zeros */ 
  time_t start_,end_;
  /*解析命令参数*/ 
  start_=clock();
  if( argc < 3  || argc > 3 || !strcmp(argv[1], "-?") ) { 
    (void)printf("hint: nweb Port-Number Top-Directory\t\tversion %d\n\n" 
  "\tnweb is a small and very safe mini web server\n" 
  "\tnweb only servers out file/web pages with extensions named below\n" 
  "\t and only from the named directory or its sub-directories.\n" 
  "\tThere is no fancy features = safe and secure.\n\n" 
  "\tExample:webserver 8181 /home/nwebdir &\n\n" 
  "\tOnly Supports:", VERSION); 
 
    for(i=0;extensions[i].ext != 0;i++) 
      (void)printf(" %s",extensions[i].ext); 
 
    (void)printf("\n\tNot Supported: URLs including \"..\", Java, Javascript, CGI\n" 
  "\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin \n" 
  "\tNo warranty given or implied\n\tNigel Griffiths nag@uk.ibm.com\n"  ); 
    exit(0); 
  } 
  if( !strncmp(argv[2],"/"   ,2 ) || !strncmp(argv[2],"/etc", 5 ) || 
      !strncmp(argv[2],"/bin",5 ) || !strncmp(argv[2],"/lib", 5 ) || 
      !strncmp(argv[2],"/tmp",5 ) || !strncmp(argv[2],"/usr", 5 ) || 
      !strncmp(argv[2],"/dev",5 ) || !strncmp(argv[2],"/sbin",6) ){ 
    (void)printf("ERROR: Bad top directory %s, see nweb -?\n",argv[2]); 
    exit(3); 
  } 
  if(chdir(argv[2]) == -1){ 
    (void)printf("ERROR: Can't Change to directory %s\n",argv[2]); 
    exit(4); 
  } 
  end_=clock();
  char s[50];
  double time_used = ((double)(start_-end_)/CLOCKS_PER_SEC);
  timming("解析命令参数",start_,s,socketfd);

  /* 建立服务端侦听socket*/ 
  if((listenfd = socket(AF_INET, SOCK_STREAM,0)) <0) 
    logger(ERROR, "system call","socket",0); 
  port = atoi(argv[1]); 
  if(port < 0 || port >60000) 
    logger(ERROR,"Invalid port number (try 1->60000)",argv[1],0); 
  serv_addr.sin_family = AF_INET; 
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
  serv_addr.sin_port = htons(port); 
  if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0) 
    logger(ERROR,"system call","bind",0); 
  if( listen(listenfd,64) <0) 
    logger(ERROR,"system call","listen",0); 
  for(hit=1; ;hit++) { 
    length = sizeof(cli_addr); 
    if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0) 
      logger(ERROR,"system call","accept",0); 
    
    int pid = fork();
    if (pid < 0) {
        // fork失败，记录错误日志
        logger(ERROR, "system call", "fork", socketfd);
        close(socketfd); // 关闭socket
    } else if (pid == 0) {
      time_t start,end;
      start = clock();
      char s[40];//初始化字符串
      close(listenfd);//关闭监听，一个子进程仅可进行一次访问
      web(socketfd, hit);
      end = clock(); 

      double chile_time = ((double)(end-start)/CLOCKS_PER_SEC);//计算总用时

      sem_wait(mutex);
      *total_time += chile_time;//加到total_time上
      sem_post(mutex);

      sprintf(s,"pid:%d using time %lf s\n",getpid(),chile_time);//拼接字符串
      logger(LOG,NULL,s,socketfd);//写入日志字符串s
      
      exit(0);//退出子程序
    } else {
      char s[40];
      sprintf(s,"Time in all is %lf s\n",*total_time);//将总时间写入log日志 
      logger(LOG,"总计时",s,socketfd);
      close(socketfd);
    } 
  } 
}