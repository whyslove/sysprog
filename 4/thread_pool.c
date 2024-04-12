#include "thread_pool.h"
#include <unistd.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

struct thread_task
{
    thread_task_f function;
    void *arg;
    void *res;
    int status;
    struct thread_pool *pool;
    pthread_mutex_t mutex;
};

struct queue_node
{
    struct thread_task *task;
    struct queue_node *next;
};

struct thread_pool
{
    pthread_t *threads;
    int *reserved;
    int max_threads;
    int threads_count;
    int threads_created;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_cond_t task_cond;
    struct queue_node *queue;
    struct queue_node *queue_tail;
    int queue_len;
    pthread_t base_thread;
    bool want_end;
    bool base_thread_ended;
    int num_finished_threads;
};

void *queue_loop_consumer(void *arg)
{
    struct thread_pool *pool = (struct thread_pool *)arg;
    while (1)
    {
        // printf("LOCKING HERE\n");
        pthread_mutex_lock(&pool->mutex);
        while (pool->queue == NULL || pool->want_end)
        {
            if (pool->want_end)
            {
                pool->num_finished_threads++;
                pthread_cond_broadcast(&pool->cond);
                pthread_mutex_unlock(&pool->mutex);

                return NULL;
            }
            pthread_cond_wait(&pool->cond, &pool->mutex);
        }
        // printf("Running threads %d, Queue len: %d\n", pool->threads_count, pool->queue_len);

        struct queue_node *to_free = pool->queue;
        // printf("IS QUEUE NULL %d\n", (int)(pool->queue == NULL));
        // printf("IS TASK NULL %d\n", (int)(pool->queue->task == NULL));
        struct thread_task *task = pool->queue->task;
        // printf("working with task %lld\n", (long long int)task);
        pool->queue = pool->queue->next;
        pool->queue_len--;
        pool->threads_count = pool->threads_count + 1;
        pthread_mutex_unlock(&pool->mutex);

        pthread_mutex_lock(&task->mutex);
        // printf("running: %p\n", task);
        task->status = RUNNING;
        void *res = task->function(task->arg);
        task->res = res;
        pthread_mutex_unlock(&task->mutex);

        // printf("finished: %p\n", task);

        pthread_mutex_lock(&pool->mutex);
        // printf("cannot take pool lock: %p\n", task);
        pool->threads_count = pool->threads_count - 1;
        pthread_mutex_unlock(&pool->mutex);
        pthread_mutex_lock(&task->mutex);
        // printf("cannot take task mutex: %p\n", task);
        task->status = JOINED;
        pthread_mutex_unlock(&task->mutex);

        pthread_cond_broadcast(&pool->cond);
        pthread_cond_broadcast(&pool->task_cond);
        // printf("trully finished: %p\n", task);
        free(to_free);
    }
}

int thread_pool_new(int max_thread_count, struct thread_pool **pool)
{
    if (max_thread_count > TPOOL_MAX_THREADS || max_thread_count < 1)
    {
        return TPOOL_ERR_INVALID_ARGUMENT;
    }

    *pool = malloc(sizeof(struct thread_pool));

    (*pool)->max_threads = max_thread_count;
    (*pool)->threads_count = 0;
    (*pool)->threads_created = 0;
    (*pool)->queue = NULL;
    (*pool)->queue_tail = NULL;
    (*pool)->threads = calloc(max_thread_count, sizeof(pthread_t));
    (*pool)->reserved = calloc(max_thread_count, sizeof(int));
    (*pool)->want_end = false;
    (*pool)->base_thread_ended = false;
    (*pool)->num_finished_threads = 0;
    pthread_cond_init(&((*pool)->cond), NULL);
    pthread_cond_init(&((*pool)->task_cond), NULL);
    pthread_mutex_init(&((*pool)->mutex), NULL);

    return 0;
}

int thread_pool_thread_count(const struct thread_pool *pool)
{
    return pool->threads_created;
}

int thread_pool_delete(struct thread_pool *pool)
{
    pthread_mutex_lock(&pool->mutex);
    if (pool->queue != NULL || pool->threads_count != 0)
    {
        printf("%d %d", pool->queue != NULL, pool->threads_count != 0);
        pthread_mutex_unlock(&pool->mutex);
        return TPOOL_ERR_HAS_TASKS;
    }

    pool->want_end = true;
    pthread_cond_broadcast(&pool->cond);
    while (pool->num_finished_threads != pool->threads_created)
    {
        pthread_cond_wait(&pool->cond, &pool->mutex);
    }
    pthread_mutex_unlock(&pool->mutex);

    // printf("DELETING POOL\n");
    free(pool->threads);
    free(pool->reserved);
    free(pool);
    return 0;
}

int thread_pool_push_task(struct thread_pool *pool, struct thread_task *task)
{
    pthread_mutex_lock(&pool->mutex);
    if (pool->queue_len == TPOOL_MAX_TASKS)
    {
        pthread_mutex_unlock(&pool->mutex);
        return TPOOL_ERR_TOO_MANY_TASKS;
    }

    pthread_mutex_lock(&task->mutex);
    // printf("pushed task %lld\n", (long long int)task);
    // printf("pushing %p\n", task);

    struct queue_node *node = malloc(sizeof(struct queue_node));
    task->pool = pool;
    task->status = SCHEDULED;
    node->task = task;
    node->next = NULL;

    if (pool->queue == NULL)
    {
        pool->queue = node;
        pool->queue_tail = node;
        pool->queue_len = 1;
    }
    else
    {
        pool->queue_tail->next = node;
        pool->queue_tail = node;
        pool->queue_len = pool->queue_len + 1;
    }

    // printf("queuelen result: %d\n", pool->queue_len);

    if (pool->threads_count == pool->threads_created && pool->threads_count != pool->max_threads)
    {
        pool->threads_created = pool->threads_created + 1;
        pthread_t ptask;
        pthread_create(&ptask, NULL, queue_loop_consumer, pool);
    }

    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);
    pthread_mutex_unlock(&task->mutex);

    return 0;
}

int thread_task_new(struct thread_task **task, thread_task_f function, void *arg)
{
    *task = malloc(sizeof(struct thread_task));
    if (*task == NULL)
    {
        return -1;
    }

    (*task)->status = NOT_SCHEDULED;
    (*task)->function = function;
    (*task)->arg = arg;
    (*task)->res = NULL;
    pthread_mutex_init(&(*task)->mutex, NULL);

    return 0;
}

bool thread_task_is_finished(const struct thread_task *task)
{
    return task->status == JOINED;
}

bool thread_task_is_running(const struct thread_task *task)
{
    return task->status == RUNNING;
}

int thread_task_join(struct thread_task *task, void **result)
{
    if (task->status == NOT_SCHEDULED)
    {
        return TPOOL_ERR_TASK_NOT_PUSHED;
    }
    // printf("joining %p\n", task);
    pthread_mutex_lock(&task->mutex);
    while (task->status != JOINED)
    {
        pthread_cond_wait(&task->pool->task_cond, &task->mutex);
    }
    *result = task->res;
    task->status = JOINED_BY_USER;
    pthread_mutex_unlock(&task->mutex);

    return 0;
}

#ifdef NEED_TIMED_JOIN

int thread_task_timed_join(struct thread_task *task, double timeout, void **result)
{
    /* IMPLEMENT THIS FUNCTION */
    (void)task;
    (void)timeout;
    (void)result;
    return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif

int thread_task_delete(struct thread_task *task)
{
    pthread_mutex_lock(&task->mutex);
    if (task->status == JOINED_BY_USER || task->status == NOT_SCHEDULED)
    {
        pthread_mutex_unlock(&task->mutex);
        // printf("freed %lld\n", (long long int)task);
        free(task);
        return 0;
    }
    pthread_mutex_unlock(&task->mutex);
    return TPOOL_ERR_TASK_IN_POOL;
}

#ifdef NEED_DETACH

int thread_task_detach(struct thread_task *task)
{
    /* IMPLEMENT THIS FUNCTION */
    (void)task;
    return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif
