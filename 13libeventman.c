#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <event2/event.h>

//退出信号回调
void signal_cb(evutil_socket_t sig, short event, void* arg){
	struct event_base* base = (struct event_base*)arg;
	event_base_loopexit(base, NULL);
}

//读回调
void readcb(evutil_socket_t fd, short event, void* arg){
	struct event* ev = (struct event*)arg;
	char buf[1024] = {0};
	int n = read(fd, buf, sizeof(buf)-1);
	if (n>0) {
		buf[n] = '\0';
		printf("客户端【%d】：%s\n", fd, buf);
	}else {
		perror("error");
		event_del(ev);
		event_free(ev);
		close(fd);
	}
}
void accept_cb(evutil_socket_t fd, short events, void* arg){
	//接收客户端fd
	struct event_base* base = (struct event_base*)arg;
	struct sockaddr_in client_addr = {0};
	socklen_t clen = sizeof(client_addr);

	evutil_make_socket_nonblocking(fd);
	int client_fd = accept(fd, (struct sockaddr *)&client_addr, &clen);

	//初始化上树节点
	struct event* ev_client = event_new(base, client_fd, events, readcb, NULL);

	//补充回调函数参数(自身节点)
	event_assign(ev_client, base, client_fd, events, readcb, ev_client);
	
	//上树监听
	event_add(ev_client, NULL);
}

int main(){
	//创建套接字
	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in server_addr = {0};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(8080);
	if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr))<0) {
		perror("bind error");
		return -1;
	}
	listen(listen_fd, 128);
	//libevent的跨平台非阻塞
	evutil_make_socket_nonblocking(listen_fd);

	//创建event根节点
	struct event_base* base = event_base_new();
	//初始化信号节点
	struct event* ev_sig = evsignal_new(base, SIGINT, signal_cb, base);
	event_add(ev_sig, NULL);

	//初始化上树节点
	struct event* ev_listen = event_new(base, listen_fd, EV_READ|EV_PERSIST, accept_cb, base);

	//上树监听
	event_add(ev_listen, NULL);
	printf("libevent server 启动~\n");

	//循环监听
	event_base_dispatch(base);
	
	//销毁根节点
	event_base_free(base);
	return 0;
}
