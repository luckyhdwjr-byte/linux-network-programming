#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>
#include <stdlib.h>
#include <arpa/inet.h>

int main(){
	// 创建套接字
	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in server_addr = {0};
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(8080);

	if(bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr))<0){
		perror("bind error");
		exit(1);
	}
	listen(listen_fd, 5);
	printf("服务端正在监听\n");

	// 配置初始位图
	fd_set tem_set, back_set;
	FD_ZERO(&back_set); // 清空位图
	FD_SET(listen_fd, &back_set); // 监听描述符亮灯

	int max_fd = listen_fd;

	while (1) {
		tem_set = back_set;

		select(max_fd+1, &tem_set, NULL, NULL, NULL);
		for (int i = 0; i <= max_fd; i++) {
			if(FD_ISSET(i, &tem_set)){
				if (i == listen_fd) { // 新客人
						      // 接待新客人
					struct sockaddr_in client_addr = {0};
					socklen_t clen = sizeof(client_addr);
					int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &clen);
					// 加入管理对象
					FD_SET(client_fd, &back_set);
					if (max_fd < client_fd) max_fd = client_fd;

				}else { // 旧客人
					char msg[1024];
					int n = read(i, msg, sizeof(msg)-1);
					if (n > 0) {
						msg[n] = '\0';
						printf("收到信息：%s\n", msg);
					}else if(n == 0){
						printf("客户端断开连接\n");
						close(i);
						FD_CLR(i, &back_set);
					}else if (n < 0) {
						perror("read ERROR");
						close(i);
						FD_CLR(i, &back_set);
					}
				}
			}
		}
	}
}
