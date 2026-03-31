#include <stdio.h>
#include <iostream>
#include <vector>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <map>
#include <fcntl.h>
#include <error.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <mutex>
#include <signal.h>

#define MAX_MONITOR 1024

volatile sig_atomic_t stop_server = 1;

void singlehandle(int sig){
	stop_server = 0;
}

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


void set_nonblock(int fd){
	int flags = fcntl(fd, F_GETFL, 0); // 获取fd的初始设置
	fcntl(fd, F_SETFL, flags | O_NONBLOCK); //设置非阻塞
}

class epollEngine{
	public:
		epollEngine() : epfd(epoll_create1(0)), events(MAX_MONITOR){
			if(epfd == -1) {
				perror("epoll error");
				exit(1);
			}
		}

		~epollEngine(){
			if (epfd != -1) close(epfd);
		}

		void add_fd(int fd, bool oneshot = false);
		int del_fd(int fd);
		int mod_fd(int fd);
		int wait(int flags);
		struct epoll_event& getclientfd(int i);

	private:	
		int epfd;
		std::vector<struct epoll_event> events;
};

void epollEngine::add_fd(int fd, bool oneshot){
	struct epoll_event ee = {0};
	ee.data.fd = fd;
	ee.events = EPOLLIN | EPOLLET;
	if (oneshot) ee.events |= EPOLLONESHOT;
	epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ee);
}
int epollEngine::del_fd(int fd){
	epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
	return 0;
}
int epollEngine::mod_fd(int fd){
	struct epoll_event ee = {0};
	ee.data.fd = fd;
	ee.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
	epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ee);
	return 0;
}
int epollEngine::wait(int flags){
	int en = epoll_wait(epfd, events.data(), MAX_MONITOR, flags);
	return en;
}
struct epoll_event& epollEngine::getclientfd(int i){
	return events[i];
}

class reactorServer{
	public:
		reactorServer(int port) : port_(port) {}
		~reactorServer(){}

		void start();
	private:
		int init_socket();
		void run_loop(int listen_fd);
		void handle_event();
		void handlenew_client(int listen_fd);
		void handleold_client(int fd);
		static void handle_static(void* arg);
		static void handlenew_static(void* arg);
	private:
		int listen_fd;
		int port_;
		threadpool_t* pool;
		std::map<int, std::string> mmsg;
		std::mutex msg_mutex;
		epollEngine engine;
};	

typedef struct {
	reactorServer* p;
	int fd;
}task;
void reactorServer::handle_static(void* arg){
	task* ta = (task*)arg;
	ta->p->handleold_client(ta->fd);
	delete ta;
}

void reactorServer::handlenew_static(void* arg){
	task* ta = (task*)arg;
	ta->p->handlenew_client(ta->fd);
	delete ta;
}

int reactorServer::init_socket(){
	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	set_nonblock(listen_fd);

	struct sockaddr_in server_addr = {0};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port_);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		perror("bind error");
		exit(1);
	}
	listen(listen_fd, 128);
	std::cout << "服务器开始监听" << std::endl;
	engine.add_fd(listen_fd, false);
	return 0;
}
void reactorServer::run_loop(int listen_fd){
	pool = threadpool_creat(5, 12); // 创建线程池
	
	signal(SIGINT, singlehandle);

	while (stop_server) {
		int en = engine.wait(2000);
		for (int i = 0; i < en; i++) {
			struct epoll_event& ev = engine.getclientfd(i);
			int fd = ev.data.fd;
			if (fd == listen_fd) {
				task* arg = new task{this,listen_fd};
				threadpool_add(pool, reactorServer::handlenew_static, (void *)arg);	
			}else {
				//	handleold_client(fd);
				task* arg = new task{this,fd};
				threadpool_add(pool, reactorServer::handle_static, (void *)arg);
			}
		}
	}
	resourceRe(pool);
	close(listen_fd);
}
void reactorServer::handlenew_client(int listen_fd){
	while (1) {
		struct sockaddr_in client_addr = {0};
		socklen_t clen = sizeof(client_addr);
		int client_fd = accept(listen_fd,
				(struct sockaddr *)&client_addr, &clen);
		if (client_fd > 0) {
			set_nonblock(client_fd);
			engine.add_fd(client_fd, true);
		}else {
			break;
		}
	}
}

void reactorServer::handleold_client(int fd){
	char msg[10] = {0};
	while (1) {
		int n = read(fd, msg, sizeof(msg));
		if (n > 0) {
			std::lock_guard<std::mutex> lock(msg_mutex);//上锁
			mmsg[fd].append(msg, n);
		}else if(n == 0){
			std::lock_guard<std::mutex> lock(msg_mutex); // 上锁
			if (!mmsg[fd].empty()) {
				std::string cids = "【客户端：" + 
					std::to_string(fd) + "】";
				std::cout << cids + mmsg[fd] << std::endl;;
				mmsg[fd].clear();
			}
			std::string s =  "【客户端：" + std::to_string(fd) + 
				"】断开连接";
			std::cout << s << std::endl;
			close(fd);
			engine.del_fd(fd);
			break;
		}else {
			if (errno == EAGAIN || EWOULDBLOCK) {
				std::lock_guard<std::mutex> lock(msg_mutex);//上锁
				if (!mmsg[fd].empty()) {
					std::string cids = "【客户端：" + 
						std::to_string(fd) + "】";
					std::cout << cids + mmsg[fd] << std::endl;
					mmsg[fd].clear();
					break;
				}
			}
			std::cerr << "read error" << std::endl;
			close(fd);
			break;
		}
		engine.mod_fd(fd);
	}
}	

void reactorServer::start(){
	if(init_socket()) return;
	std::cout << "reactor Server启动【" << port_ << "】" << std::endl;
	run_loop(listen_fd);	
}

int main(){
	reactorServer server(8080);
	server.start();
	// threadpool_t* pool = threadpool_creat(5, 12);

	/*for (int i = 0; i < 10; i++) {
	  int* arg = (int*)malloc(sizeof(int));
	 *arg = i;
	 threadpool_add(pool, worktask, (void *)arg);
	 }*/
	sleep(5);

	//	resourceRe (pool);
	return 0;
}

