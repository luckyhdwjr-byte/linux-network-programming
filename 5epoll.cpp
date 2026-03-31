#include <iostream>
#include <vector>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <map>
#include <errno.h>
#include <fcntl.h>

#define MAX_MONITOR 1024

void set_nonblock(int fd){
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(){
	// 创建套接字
	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

	set_nonblock(listen_fd);

	struct sockaddr_in server_addr = {0};
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(8080);

	if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr))<0) {
		perror("bind error");
		exit(1);
	}
	listen(listen_fd, 128);
	std::cout << "服务器开始监听" << std::endl;

	// 创建epoll
	int ep_fd = epoll_create1(0);	

	struct epoll_event ee = {0};
	ee.data.fd = listen_fd;
	ee.events = EPOLLIN | EPOLLET; // 可读和边缘触发

	epoll_ctl(ep_fd, EPOLL_CTL_ADD, listen_fd, &ee);
	// 创建动态数组
	std::vector<struct epoll_event> events(MAX_MONITOR);
	std::map<int, std::string> mmsg;

	std::cout << "epoll服务器启动" << std::endl;

	while (1) {
		int en = epoll_wait(ep_fd, events.data(), MAX_MONITOR, -1); // 阻塞监控

		// 循环处理
		for (int i = 0; i < en; i++) {
			int fd = events[i].data.fd;
			if (fd == listen_fd) { // 新客人
				while (1) {
					// 接受客户端fd
					struct sockaddr_in client_addr = {0};
					socklen_t clen = sizeof(client_addr);
					int client_fd = accept(listen_fd,
							(struct sockaddr *)&client_addr, &clen);
					if (client_fd > 0) {
						// 存入监控名单
						set_nonblock(client_fd);
						ee.data.fd = client_fd;
						ee.events = EPOLLIN | EPOLLET;
						epoll_ctl(ep_fd, EPOLL_CTL_ADD, client_fd, &ee);
					}else {
						break;	
					}
				}
			}else { // 旧客人
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
						epoll_ctl(ep_fd, EPOLL_CTL_DEL, fd, &ee);
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
		}	
	}
	return 0;
}

