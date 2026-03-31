#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

enum httpstate{STATE_LINE, STATE_HEADER, STATE_BODY, STATE_DONE};

struct http_session{
	enum httpstate state;
	char method[16];
	char url[256];
};

//发送错误响应
void send_error(struct bufferevent* bev, int code, const char* msg){
	 char buf[512] = {0};
	 sprintf(buf, "HTTP/1.1 %d %s\r\n"
		      "Content-Length: 0\r\n"
		      "Connection: close\r\n"
		      "\r\n",
		      code, msg);
	bufferevent_write(bev, buf, strlen(buf));
}

// 发送文件
void send_file(struct bufferevent* bev, char* path){
	//路径预处理
	char* real_path = path + 1;
	if (strlen(real_path) == 0) real_path = "index.html";

	//获取文件信息
	struct stat st;
	if (stat(real_path, &st)<0 || !S_ISREG(st.st_mode)) {
		send_error(bev, 404, "NOT FOUND");
		return;
	}
	
	//拼接、发送响应头
	char header[256] = {0};
	sprintf(header, "HTTP/1.1 200 OK\r\n"         // 1. 状态行：告诉浏览器请求成功
   			"Content-Length: %ld\r\n"      // 2. 告诉浏览器：后面跟着的文件有 st_size 那么大
    			"Connection: close\r\n"        // 3. 告诉浏览器：发完这一票咱就断开
    			"\r\n",
			st.st_size);
	bufferevent_write(bev, header, strlen(header));
	//发送文件
	int fd = open(real_path, O_RDONLY | 0644);
	if(fd < 0) return;
	//sendfile(bufferevent_getfd(bev), fd, NULL, st.st_size);
	//close(fd);
	struct evbuffer* output = bufferevent_get_output(bev);
	evbuffer_add_file(output, fd, 0, st.st_size);
}

//退出信号回调
void signal_cb(evutil_socket_t sig, short event, void* arg){
	struct event_base* base = (struct event_base*)arg;

	event_base_loopexit(base, NULL);
}

//读回调
void readcb(struct bufferevent* bev, void* arg){
	struct http_session* session = (struct http_session*)arg;
	struct evbuffer* input = bufferevent_get_input(bev);

	while (session->state != STATE_DONE) {
		size_t len;
		char* line = evbuffer_readln(input, &len, EVBUFFER_EOL_CRLF);
		if (!line) break;  //不够一行返回	

		switch (session->state) {
		case STATE_LINE:
			sscanf(line, "%s %s", session->method, session->url);
			session->state = STATE_HEADER;
			break;
		case STATE_HEADER:
			if (len == 0) session->state = STATE_DONE;
			break;
		default:break;
		}
		free(line);
	}
	if (session->state == STATE_DONE) {
		send_file(bev, session->url);
		session->state = STATE_LINE;
	}
}

void eventcb(struct bufferevent* bev, short events, void* arg){
	if (events & BEV_EVENT_EOF) {
		printf("客户端优雅地断开了。\n");
	} else if (events & BEV_EVENT_ERROR) {
		printf("底层发生致命错误，强制断开。\n");
	}

	free(arg);
	bufferevent_free(bev); //断开释放资源
}

void listener_cb(struct evconnlistener* listener, evutil_socket_t fd,
		struct sockaddr* sa, int socklen,void* arg){
	struct event_base* base = (struct event_base*)arg;

	//创建记忆块
	struct http_session* session = malloc(sizeof(struct http_session));
	memset(session, 0, sizeof(struct http_session));
	session->state = STATE_LINE;

	//初始化上树节点（自动读写）
	struct bufferevent* bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	//设置水位线
	//bufferevent_setwatermark(bev, EV_READ, 4, 0);

	//绑定回调
	bufferevent_setcb(bev, readcb, NULL, eventcb, session);
	//上树监听
	bufferevent_enable(bev, EV_READ); // 不用单独监听断开事件，读事件会返回0，然后回调
}
int main(){
	//创建event根节点
	struct event_base* base = event_base_new();

	struct event* evs = evsignal_new(base, SIGINT, signal_cb, base);
	event_add(evs, NULL);

	//配置监听地址
	struct sockaddr_in addr ={0};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080);

	//socket bind listen accept一条龙
	struct evconnlistener* listener = evconnlistener_new_bind(base, listener_cb, base, LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1, (struct sockaddr *)&addr, sizeof(addr));

	//循环监听
	event_base_dispatch(base);

	evconnlistener_free(listener);
	event_free(evs);
	event_base_free(base);
	printf("正在退出\n");
	return 0;
}

