#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

int main(){
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	
	struct sockaddr_in server_addr = {0};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(8080);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
		perror("bind error");
		return -1;
	}

	struct sockaddr_in client_addr = {0};
	while (1) {
		char buf[1024] = {0};
		socklen_t clen = (socklen_t)sizeof(client_addr);
		int n = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&client_addr, &clen);	
		if (n > 0) {
			printf("【%s】：%s\n", inet_ntoa(client_addr.sin_addr), buf);
		}
		sendto(fd, buf, n, 0, (struct sockaddr *)&client_addr, clen);
	}
	return 0;
}
