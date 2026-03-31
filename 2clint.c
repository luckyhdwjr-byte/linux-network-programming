#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

int main(){
	int sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd == -1) {
		perror("socket error");
		exit(1);
	}
	struct sockaddr_in clinet_addr = {0}; // 命名错了，这里其实是服务器地址

	clinet_addr.sin_family = AF_INET;
	clinet_addr.sin_port = htons(8080);
	clinet_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (connect(sfd, (struct sockaddr *)&clinet_addr, sizeof(clinet_addr)) < 0) {
		perror("connect error");
		exit(1);
	}
	char buf[] = "你好，服务端";
	send(sfd, buf, strlen(buf), 0);
	printf("消息已发送\n");

	close(sfd);
	return 0;
}
