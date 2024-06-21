#include<stdbool.h>
#include<stdlib.h>
#define BUFFERSIZE 8096
#define MAXPOOL 20
typedef struct {
    int fd;
    char *buffer;
}page;

typedef struct {
    int len;
    page **pages;
    page *pagehead;
    page *pageend;
}buffer_pool;

buffer_pool *initpool(){
    buffer_pool *pfpool = malloc(sizeof(buffer_pool));
    pfpool->len=0;
}

void push2buffer(page * page,buffer_pool * bfpool){
    if(bfpool->len==0){
        bfpool->pagehead=page;
        bfpool->pageend=page;
        bfpool->len=1;
        return;
    }
    
}