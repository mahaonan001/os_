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