#include "thread_pool.h"
#include <unistd.h>

#include <pthread.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

struct thread_task {
	thread_task_f function;
	void *arg;
	int status;
	struct thread_pool *pool;
	int thread_number;
	pthread_t thread;
};

struct queue_node {
    struct thread_task *task;
    struct queue_node *next;
};

struct thread_pool {
	pthread_t *threads;
	int* reserved;
	int max_threads;
	int threads_count;
	int threads_created;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	struct queue_node *queue;
	struct queue_node* queue_tail;
	int queue_len;
	pthread_t base_thread;
	bool want_end;
	bool base_thread_ended;
};

struct thread_args_enclosed {
	struct thread_task* task;
};


void *enclose_thread_task(void *arg) {
	struct thread_args_enclosed *args_enclosed = (struct thread_args_enclosed*)arg;

	struct thread_task *task = args_enclosed->task;
    void *result = task->function(task->arg);
	
	pthread_mutex_lock(&task->pool->mutex);
	task->pool->reserved[task->thread_number] = 0;
	task->status = JOINED;
	task->pool->threads_count--;
	pthread_cond_broadcast(&task->pool->cond);
	pthread_mutex_unlock(&task->pool->mutex);

	free(args_enclosed);
	return result;
}

void* queue_loop_consumer(void* arg) {
	struct thread_pool *pool = (struct thread_pool*)arg;
	while (1) {
		pthread_mutex_lock(&pool->mutex);
		while(pool->queue == NULL || pool->threads_count == pool->max_threads || pool->want_end) {
			if (pool->want_end) {
				pool->base_thread_ended = true;
				pthread_cond_broadcast(&pool->cond);
				pthread_mutex_unlock(&pool->mutex);
				return NULL;
			}
			pthread_cond_wait(&pool->cond, &pool->mutex);
		}

		if (pool->threads_count == pool->threads_created && pool->threads_count != pool->max_threads)
			pool->threads_created++;

		int free_index = 0;
		for (int i = 0; i<pool->max_threads; i++) {
			if (pool->reserved[i] == 0) {
				free_index = i;
			}
		}

		struct thread_args_enclosed* args_enclosed = malloc(sizeof(struct thread_args_enclosed));
		struct queue_node *to_free = pool->queue;
		struct thread_task *task = pool->queue->task;
		args_enclosed->task = task;
		pool->queue = pool->queue->next;
		pool->queue_len--;
		task->thread = pool->threads[free_index];
		pthread_create(&task->thread, NULL, enclose_thread_task, args_enclosed);
		free(to_free);
		pthread_cond_broadcast(&task->pool->cond);

		pool->reserved[free_index] = 1;
		task->status = RUNNING;
		task->thread_number = free_index;
		task->pool->threads_count++;
		pthread_mutex_unlock(&pool->mutex);
	}
}


int
thread_pool_new(int max_thread_count, struct thread_pool **pool)
{
	if (max_thread_count > TPOOL_MAX_THREADS || max_thread_count < 1){
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
	pthread_cond_init(&((*pool)->cond), NULL);
	pthread_mutex_init(&((*pool)->mutex), NULL);


	pthread_create(&(*pool)->base_thread, NULL, queue_loop_consumer, *pool);

	return 0;
}

int
thread_pool_thread_count(const struct thread_pool *pool)
{
	return pool->threads_created;
}

int
thread_pool_delete(struct thread_pool *pool)
{
	if (pool->queue != NULL || pool->threads_count != 0)
		return TPOOL_ERR_HAS_TASKS;

	if (pool->threads_count == 0) {
		pthread_mutex_lock(&pool->mutex);
		pool->want_end = true;
		pthread_cond_broadcast(&pool->cond);
		while (pool->base_thread_ended == false) {
			pthread_cond_wait(&pool->cond, &pool->mutex);
		}
		pthread_mutex_unlock(&pool->mutex);

		free(pool->threads);
		free(pool->reserved);
		free(pool);
		return 0;
	}


	// pthread_mutex_destroy(&(pool->mutex));
	// pthread_cond_destroy(&(pool->cond));

	return 123124;	
}

int
thread_pool_push_task(struct thread_pool *pool, struct thread_task *task)
{
	pthread_mutex_lock(&pool->mutex);
	if (pool->queue_len == TPOOL_MAX_TASKS) {
		pthread_mutex_unlock(&pool->mutex);
		return TPOOL_ERR_TOO_MANY_TASKS;
	}

	struct queue_node* node = malloc(sizeof(struct queue_node));

	if (pool->queue == NULL) {
		pool->queue = node;
		pool->queue_tail = node;
		pool->queue_len = 1;
	} else {
		pool->queue_tail->next = node;
		pool->queue_tail = node;
		pool->queue_len++;
	}

	node->task = task;
	task->status = SCHEDULED;
	task->pool = pool;
	pthread_mutex_unlock(&pool->mutex);
	pthread_cond_broadcast(&task->pool->cond); // нужно для джойнов

	return 0;
}

int
thread_task_new(struct thread_task **task, thread_task_f function, void *arg)
{
	*task = malloc(sizeof(struct thread_task));
    if (*task == NULL) {
        return -1;
    }

	(*task)->status = NOT_SCHEDULED;
	(*task)->function = function;
    (*task)->arg = arg;

	return 0;
}

bool
thread_task_is_finished(const struct thread_task *task)
{
	return task->status == JOINED;
}

bool
thread_task_is_running(const struct thread_task *task)
{
	return task->status == RUNNING;
}

int
thread_task_join(struct thread_task *task, void **result)
{
	if (task->status == NOT_SCHEDULED) {
		return TPOOL_ERR_TASK_NOT_PUSHED;
	}
	pthread_mutex_lock(&task->pool->mutex);
	while(task->status <= SCHEDULED) {
		pthread_cond_wait(&task->pool->cond, &task->pool->mutex);
	}
	
	pthread_mutex_unlock(&task->pool->mutex);

	int res = pthread_join(task->thread, result);
	if (res != 0) {
		return res;
	}

	return 0;
}

#ifdef NEED_TIMED_JOIN

int
thread_task_timed_join(struct thread_task *task, double timeout, void **result)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	(void)timeout;
	(void)result;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif

int
thread_task_delete(struct thread_task *task)
{
	if (task->status == JOINED || task->status == NOT_SCHEDULED) {
		free(task);
		return 0;
	}
	return TPOOL_ERR_TASK_IN_POOL;
}

#ifdef NEED_DETACH

int
thread_task_detach(struct thread_task *task)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif
