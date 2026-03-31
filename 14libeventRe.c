#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

//退出信号回调
void signal_cb(evutil_socket_t sig, short event, void* arg){
	struct event_base* base = (struct event_base*)arg;
	
	event_base_loopexit(base, NULL);
}
void readcb(struct bufferevent* bev, void* arg){
	struct evbuffer* input = bufferevent_get_input(bev);

	evutil_socket_t fd = bufferevent_getfd(bev);
	while(1){
		size_t sz = evbuffer_get_length(input);
		if (sz < 4) break;;

		unsigned int len;
		evbuffer_copyout(input, &len, 4);

		len = ntohl(len);
		if (sz < (len+4)) break;;

		//通过判断，确认满包
		evbuffer_drain(input, 4);
		char* buf = malloc(len + 1);
		evbuffer_remove(input, buf, len);
		buf[len] = '\0';

		printf("【%d】：%s\n", fd, buf);

		bufferevent_write(bev, buf, len);
		free(buf);
	}
}

void eventcb(struct bufferevent* bev, short events, void* arg){
	if (events & BEV_EVENT_EOF) {
		printf("客户端优雅地断开了。\n");
	} else if (events & BEV_EVENT_ERROR) {
		printf("底层发生致命错误，强制断开。\n");
	}

	bufferevent_free(bev); //断开释放资源
}

void listener_cb(struct evconnlistener* listener, evutil_socket_t fd,
		struct sockaddr* sa, int socklen,void* arg){
	struct event_base* base = (struct event_base*)arg;

	//初始化上树节点（自动读写）
	struct bufferevent* bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	//设置水位线
	bufferevent_setwatermark(bev, EV_READ, 4, 0);
	//绑定回调
	bufferevent_setcb(bev, readcb, NULL, eventcb, NULL);
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
