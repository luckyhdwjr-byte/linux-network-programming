#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

void handle_client(int client_fd){
	pid_t pid = fork();
	if (pid == 0) { //子进程发送信息
		char send_buf[1024];
		while (1) {
			if (fgets(send_buf, sizeof(send_buf), stdin) != NULL) {
				// 1. 先用之前的逻辑把末尾回车干掉
				send_buf[strcspn(send_buf, "\n")] = '\0';

				// 2. 🌟 解决“多出的字”和乱码的关键：
				// 从前往后扫描，如果发现 ASCII 值小于 32 的非正常字符（退格残余 ^H 等），
				// 或者是导致乱码的控制符，直接把该位置及其后面全部切断。
				int len = strlen(send_buf);
				for (int i = 0; i < len; i++) {
					// 127 是退格产生的常见乱码位
					if ((unsigned char)send_buf[i] < 32 || (unsigned char)send_buf[i] == 127) {
						send_buf[i] = '\0';
						break;
					}
				}

				// 3. 只有真正有内容才发
				if (strlen(send_buf) > 0) {
					send(client_fd, send_buf, strlen(send_buf), 0);
				}
			}
			// 清空缓冲区
			memset(send_buf, 0, sizeof(send_buf));
			//	if (fgets(send_buf, sizeof(send_buf), stdin) != NULL) {
			//		// 🌟 核心操作：找到末尾的换行符 '\n' 并用 '\0' 替换掉。
			//		// 这样可以确保发送的数据是一个干净的、没有多余不可见字符的 C 字符串。
			//		send_buf[strcspn(send_buf, "\n")] = '\0';
			//		if (strlen(send_buf) > 0) {
			//			if (send(client_fd, send_buf, strlen(send_buf), 0) == -1) {
			//				perror("send error");
			//				exit(1);
			//			}

			//		}

			//	}		
		}
	}else if (pid > 0) { //父进程接收信息
		char buf[1024];
		while (1) {
			memset(buf, 0, sizeof(buf));
			int n = recv(client_fd, buf, sizeof(buf)-1, 0);
			if (n > 0) {
				buf[n] = '\0';
				printf("\n[子进程：%d]读取到数据：%s", getpid(), buf);
				fflush(stdout); // 强制刷新缓冲区
						//send(client_fd, "I received your message!", 24, 0);
			}else if (n == 0) {
				printf("客户端断开连接\n");
				kill(pid, SIGKILL);
				break;
			}else if (n < 0) {
				perror("read error");
				kill(pid, SIGKILL);
				break;
			}
		}
	}
}

int main(){
	// 信号屏蔽
	signal(SIGCHLD, SIG_IGN);
	// 创建监听套接字
	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in server_addr = {0};

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(8080);

	if(bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
		perror("bind error");
		exit(1);
	}
	listen(listen_fd, 5);
	printf("并发服务器启动，8080正在监听\n");

	while (1) {
		struct sockaddr_in client_addr = {0};
		socklen_t aclen = sizeof(client_addr);

		int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &aclen);
		if (client_fd < 0) continue;
		pid_t pid = fork();

		if (pid == 0) {
			close(listen_fd);
			handle_client(client_fd);
			close(client_fd);
			exit(0);
		}

		close(client_fd);
	}
	close(listen_fd);
	return 0;
}
