#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>

int udsServer(const char* path){
	int listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		perror("socket error");
		_exit(1);
	}
	unlink(path);

	struct sockaddr_un uds_addr = {0};
	uds_addr.sun_family = AF_UNIX;
	strncpy(uds_addr.sun_path, path, sizeof(uds_addr.sun_path)-1);

	if (bind(listen_fd, (struct sockaddr *)&uds_addr, sizeof(uds_addr)) < 0) {
		perror("bind error");
		_exit(1);
	}
	listen(listen_fd, 5);
	printf("UDS服务器【%d】已在\"%s\"开始监听", listen_fd, path);

	while(1){
		struct sockaddr_un client_addr = {0};
		socklen_t clen = (socklen_t)sizeof(client_addr);
		int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &clen);
		if (client_fd < 0) {
			perror("accept error");
			_exit(1);
		}

		int flags = fcntl(client_fd, F_GETFL);
		fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
		while (1) {
			char buf[1024] = {0};
			int n = read(client_fd, buf, sizeof(buf));
			if (n == 0) {
				printf("客户端【%d】断开连接", client_fd);
				return 0;
			}else if (n > 0) {
				printf("客户端【%d】：%s", client_fd, buf);	
			}
		}
	}
	return listen_fd;
}

int main(int argc, char* argv[]){
	udsServer(argv[1]);
	return 0;
}

