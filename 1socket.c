#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

int main(){
	// 创建套接字
	int sfd = socket(AF_INET, SOCK_STREAM, 0);
	
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(8080);
	
	// 内核绑定端口Bind
	if(bind(sfd, (struct sockaddr*)&addr, sizeof(addr)) == -1){
		perror("bind error");
		exit(1);
	}
	// 监听
	listen(sfd, 6);
	printf("服务器正在监听8080端口\n");
	
	struct sockaddr_in clientaddr;
	socklen_t addr_len = sizeof(clientaddr);

	int cfd = accept(sfd, (struct sockaddr*)&clientaddr, &addr_len);
	printf("连接已建立\n");

	char buf[1024] = {0};
	read(cfd, buf, sizeof(buf));
	printf("收到数据：%s\n", buf);

	close(sfd);
	close(cfd);

	return 0;
}
