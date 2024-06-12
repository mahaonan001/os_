#include <stdbool.h>
#include <pthread.h>
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
typedef struct {
    double total_active_time;
    double total_block_time;
    int max_active_threads;
    int min_active_threads;
    int total_threads;
    int active_threads;
    int queue_length;
    pthread_mutex_t lock;
} performance_data;
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