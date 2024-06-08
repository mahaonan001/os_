#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include<sys/prctl.h>
typedef struct staconv
{
  /* data */
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int status;
} staconv;

typedef struct task
{
  struct task *next;
  void (*function)(void *arg);
  void *arg;
} task;

typedef struct taskqueue // 任务队列
{
  /* data */
  pthread_mutex_t mutex;
  task *front;
  task *end;
  staconv *has_jobs;
  int len;
} taskqueue;

typedef struct thread
{
  /* data */
  int id;
  pthread_t pthread;       // 线程本体
  struct threadpool *pool; // 所属线程池
} thread;

typedef struct threadpool
{
  /* data */
  thread **threads;
  volatile int num_threads;        // 线程数
  volatile int num_working;        // 运行的线程数
  pthread_mutex_t thcont_lock;     // 锁
  pthread_cond_t threads_all_idle; // 销毁锁的条件变量
  taskqueue queue;
  volatile bool is_alive;
} threadpool;
void init_taskqueue(taskqueue *queue);
struct threadpool *initThreadPool(int num_threads);
void push_taskqueue(taskqueue *queue, task *curtask);
void addTask2ThreadPool(threadpool *pool, task *curtask);
void waitThreadPool(threadpool *pool);
void destoryThreadPool(threadpool *pool);
int getNumofThreadWorking(threadpool *pool);
int create_thread(struct threadpool *pool, struct thread **pthread, int id);
void destory_taskqueue(taskqueue *queue);
void thread_do(thread* pthread);
task* take_taskqueue(taskqueue *queue);
int main()
{
  return 0;
}
void init_taskqueue(taskqueue *queue)
{
  if (queue == NULL)
  {
    return; // 如果传入的队列指针为空，则直接返回
  }
  // 分配内存给 staconv 结构体，并检查是否分配成功
  staconv *staconv_ = malloc(sizeof(struct staconv));
  if (staconv_ == NULL)
  {
    perror("Failed to allocate memory for staconv");
    exit(EXIT_FAILURE); // 内存分配失败，退出程序
  }
  // 初始化 staconv_ 中的互斥锁和条件变量
  pthread_mutex_init(&(staconv_->mutex), NULL);
  pthread_cond_init(&(staconv_->cond), NULL);
  staconv_->status = 0;

  // 将 staconv_ 赋值给队列的 has_jobs 成员
  queue->has_jobs = staconv_;

  // 初始化任务队列的互斥锁
  pthread_mutex_init(&(queue->mutex), NULL);
  queue->front = NULL;
  queue->end = NULL;
  queue->len = 0;
}

struct threadpool *initThreadPool(int num_threads)
{
  threadpool *pool;
  pool = (threadpool *)malloc(sizeof(threadpool));
  pool->num_threads = num_threads;
  pool->num_working = 0;

  pthread_mutex_init(&(pool->thcont_lock), NULL);
  pthread_cond_init(&(pool->threads_all_idle), NULL);

  init_taskqueue(&pool->queue);

  pool->threads = malloc(num_threads * sizeof(struct thread *));

  for (int i = 0; i < num_threads; ++i)
  {
    create_thread(pool,&pool->threads[i],i);
  }
  while (pool->num_threads != num_threads)
  {
  }
  return pool;
}

void push_taskqueue(taskqueue *queue, task *curtask)
{
  if (queue->len == 0)
  {
    queue->front = curtask;
    queue->end = curtask;
  }
  else
  {
    queue->end->next = curtask;
    queue->end = curtask;
  }
  pthread_mutex_lock(&queue->mutex);
  queue->len += 1;
  pthread_mutex_unlock(&queue->mutex);
}

void addTask2ThreadPool(threadpool *pool, task *curtask)
{
  push_taskqueue(&pool->queue, curtask);
}

void waitThreadPool(threadpool *pool)
{
  pthread_mutex_lock(&pool->thcont_lock);
  while (pool->queue.len || pool->num_working)
  {
    pthread_cond_wait(&pool->threads_all_idle, &pool->thcont_lock);
  }
  pthread_mutex_unlock(&pool->thcont_lock);
}

void destoryThreadPool(threadpool *pool)
{

  waitThreadPool(pool);

  destory_taskqueue(&pool->queue);
  for (int i = 0; i < pool->num_threads; ++i)
  {
    free(pool->threads[i]);
  }
  free(pool);
}
int getNumofThreadWorking(threadpool *pool)
{
  return pool->num_working;
}

void destory_taskqueue(taskqueue *queue){
  free(queue->has_jobs);
  
  while(queue->front!=queue->end){
    task * task_=queue->front;
    task *se_task = queue->front->next;
    free(task_);
    queue->front=se_task;
  }
  free(queue->end);
  free(queue);
}

int create_thread(struct threadpool *pool, struct thread **pthread, int id)
{
  *pthread = (struct thread *)malloc(sizeof(struct thread));
  if (pthread == NULL)
  {
    perror("creat_thread():could not allocate memory for thread\n");
    return -1;
  }
  (*pthread)->pool = pool;
  (*pthread)->id = id;
  pthread_create(&(*pthread)->pthread,NULL,(void*)thread_do,(*pthread));
  pthread_detach((*pthread)->pthread);
  return 0;
}

task* take_taskqueue(taskqueue *queue){
  task* task_ = queue->front;
  queue->front = task_->next;
  (queue->len)--;
  return task_;
}

void thread_do(thread* pthread){
  char thread_name[128]={0};
  sprintf(thread_name,"thread_pool_%d",pthread->id);
  prctl(PR_SET_NAME,thread_name);

  threadpool* pool = pthread->pool;
  for(int i=0;i<sizeof(pool->threads)/sizeof(pool->threads[0]);i++){
    (pool->num_threads)++;
  }
  while(pool->is_alive){
    if(pool->queue.len!=0){
      waitThreadPool(pool);
    }
    if(pool->is_alive){
      for(int i=0;i<pool->queue.len;i++){
        (pool->num_working)++;
      }
      void(*func)(void*);
      void *arg;
      task* curtask = take_taskqueue(&pool->queue);
      if(curtask){
        func = curtask->function;
        arg = curtask->arg;
        func(arg);
        free(curtask);
      }
    }

  }
}