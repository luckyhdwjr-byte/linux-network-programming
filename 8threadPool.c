#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

typedef struct{
	void (*function)(void* arg);
	void* arg;
} thread_task_t;

typedef struct{
	pthread_mutex_t lock;
	pthread_cond_t not_empty;
	pthread_cond_t not_full;
	pthread_t* threads;
	thread_task_t* task_queue;

	int queue_capacity_; //最大任务数
	int queue_size; //当前任务数
	int queue_front; //队头取货
	int queue_rear; //队尾存货

	int thread_count_;
	bool shutdown;
} threadpool_t;

void* thread_worker(void* threadpool);

threadpool_t* threadpool_creat(int thread_count, int queue_capacity){
	threadpool_t* pool = (threadpool_t*)malloc(sizeof(threadpool_t));

	// 初始化基础数值
	pool->queue_capacity_ = queue_capacity;
	pool->queue_size = 0;
	pool->queue_front = 0;
	pool->queue_rear = 0;
	pool->thread_count_ = thread_count;
	pool->shutdown = false;

	// 初始化锁和条件变量
	pthread_mutex_init(&(pool->lock), NULL);
	pthread_cond_init(&(pool->not_empty), NULL);
	pthread_cond_init(&(pool->not_full), NULL);

	// 为数组申请内存
	pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * thread_count);
	pool->task_queue = (thread_task_t*)malloc(sizeof(thread_task_t) * queue_capacity);

	// 循环创建线程
	for (int i = 0; i < thread_count; i++) {
		pthread_create(&(pool->threads[i]), NULL, thread_worker, (void *)pool);
	}
	return pool;
}

// 老板派活
void threadpool_add(threadpool_t* pool, void (*func)(void* arg), void* arg){
	pthread_mutex_lock(&(pool->lock));

	while (pool->queue_size == pool->queue_capacity_ && !(pool->shutdown)) {
		pthread_cond_wait(&(pool->not_full), &(pool->lock));
	}
	if ((pool->queue_size)<(pool->queue_capacity_) && !(pool->shutdown)) {
		pool->task_queue[pool->queue_rear].function = func;
		pool->task_queue[pool->queue_rear].arg = arg;
		pool->queue_rear = ((pool->queue_rear)+1) % (pool->queue_capacity_);
		(pool->queue_size)++;
		pthread_cond_signal(&(pool->not_empty));
	}else if (pool->shutdown) {
		pthread_mutex_unlock(&(pool->lock));
		return;
	}

	pthread_mutex_unlock(&(pool->lock));
}

// 工人接活
void* thread_worker(void* threadpool){
	threadpool_t* pool = (threadpool_t*)threadpool;
	while (1) {
		pthread_mutex_lock(&(pool->lock));
		while (pool->queue_size == 0 && !(pool->shutdown)) {
			pthread_cond_wait(&(pool->not_empty), &(pool->lock));
		}
		if (pool->shutdown && pool->queue_size == 0) {
			pthread_mutex_unlock(&(pool->lock));
			pthread_exit(NULL);
		}

		thread_task_t task = pool->task_queue[pool->queue_front]; // 领到任务
		pool->queue_front = ((pool->queue_front)+1) % (pool->queue_capacity_);
		pool->queue_size--;

		pthread_cond_signal(&(pool->not_full));
		pthread_mutex_unlock(&(pool->lock));
		(*(task.function))(task.arg);
	}
	return NULL;
}

void worktask(void* arg){
	int id = *((int*)arg);
	printf("%ld号工作人员正在做%d号任务\n", pthread_self(), id);
	free(arg);
	sleep(1);
}

void resourceRe(threadpool_t* pool){
	int count = pool->thread_count_;
	pthread_mutex_lock(&(pool->lock));
	pool->shutdown = true;
	pthread_cond_broadcast(&(pool->not_empty));
	pthread_cond_broadcast(&(pool->not_full));
	pthread_mutex_unlock(&(pool->lock));

	for (int i = 0; i < count; i ++) {
		pthread_join(pool->threads[i], NULL);
	}
	free(pool->task_queue);
	free(pool->threads);

	pthread_mutex_destroy(&(pool->lock));
	pthread_cond_destroy(&(pool->not_empty));
	pthread_cond_destroy(&(pool->not_full));

	free(pool);
}

int main(){
	threadpool_t* pool = threadpool_creat(5, 12);

	for (int i = 0; i < 10; i++) {
		int* arg = (int*)malloc(sizeof(int));
		*arg = i;
		threadpool_add(pool, worktask, (void *)arg);
	}
	sleep(5);

	resourceRe (pool);
	return 0;
}
