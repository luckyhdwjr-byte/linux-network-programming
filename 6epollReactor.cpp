#include <iostream>
#include <vector>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <map>
#include <fcntl.h>
#include <error.h>

#define MAX_MONITOR 1024

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

		void add_fd(int fd);
		int del_fd(int fd);
		int wait(int flags);
		struct epoll_event& getclientfd(int i);

	private:	
		struct epoll_event ee = {0};
		int epfd;
		std::vector<struct epoll_event> events;
};

void epollEngine::add_fd(int fd){
	ee.data.fd = fd;
	ee.events = EPOLLIN | EPOLLET;
	epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ee);
}
int epollEngine::del_fd(int fd){
	epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &ee);
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
		int handlenew_client(int listen_fd);
		void handleold_client(int fd);
	private:
		int listen_fd;
		int port_;
		std::map<int, std::string> mmsg;
		epollEngine engine;
};	

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
	engine.add_fd(listen_fd);
	return 0;
}
void reactorServer::run_loop(int listen_fd){
	while (1) {
		int en = engine.wait(-1);
		for (int i = 0; i < en; i++) {
			struct epoll_event& ev = engine.getclientfd(i);
			int fd = ev.data.fd;
			if (fd == listen_fd) {
				while (1) {
					int client_fd = handlenew_client(listen_fd);
					if (client_fd > 0) {
						set_nonblock(client_fd);
						engine.add_fd(client_fd);
					}else {
						break;
					}
				}
			}else {
				handleold_client(fd);
			}
		}
	}
}
int reactorServer::handlenew_client(int listen_fd){
	// 接受客户端fd
	struct sockaddr_in client_addr = {0};
	socklen_t clen = sizeof(client_addr);
	int client_fd = accept(listen_fd,
			(struct sockaddr *)&client_addr, &clen);
	return client_fd;
}

void reactorServer::handleold_client(int fd){
	char msg[10] = {0};
	while (1) {
		int n = read(fd, msg, sizeof(msg));
		if (n > 0) {
			mmsg[fd].append(msg, n);
		}else if(n == 0){
			if (!mmsg[fd].empty()) {
				std::string cids = "【客户端：" + 
					std::to_string(fd) + "】";
				std::cout << cids + mmsg[fd] << std::endl;
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
	return 0;
}
