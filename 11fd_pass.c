#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>

int send_fd(int uds_fd, int send_to_fd){
	struct msghdr msg = {0};

	struct iovec iov[1] = {0};
	char base = 'F';
	iov[0].iov_base = &base;
	iov[0].iov_len = 1;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	char buf[CMSG_SPACE(sizeof(int))] = {0}; // CMSG_SPACE包含补齐的长度

	msg.msg_control = buf;
	msg.msg_controllen = sizeof(buf);

	struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);

	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(int)); // CMSG_LEN不包含补齐的长度

	*((int*)CMSG_DATA(cmsg)) = send_to_fd; // CMSG_DATA 返回的是存放数据的真实地址

	if (sendmsg(uds_fd, &msg, 0) < 0) {
		perror("sendmsg error");
		return -1;
	}
	return 0;
}

int rev_fd(int uds_fd){
	struct msghdr msg = {0};

	struct iovec iov[1] = {0};
	char base;
	iov[0].iov_base = &base;
	iov[0].iov_len = 1;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	char buf[CMSG_SPACE(sizeof(int))] = {0};

	msg.msg_control = buf;
	msg.msg_controllen = sizeof(buf);
	int n = recvmsg(uds_fd, &msg, 0);
	if (n < 0) {
		perror("receive error");
		return -1;
	}if (n == 0) {
		fprintf(stderr, "master closed connection\n");
		return -1;
	}
	struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);

	if (cmsg == NULL || cmsg->cmsg_type != SCM_RIGHTS) {
		perror("error: 未收到fd");
		return -1;
	}

	int recv_fd = *((int *)CMSG_DATA(cmsg));
	return recv_fd;
}

int main(){
	int sv[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
		perror("socketpair error");
		return -1;
	}
	pid_t pid = fork();
	if (pid == 0) {
		close(sv[0]);
		int fd = rev_fd(sv[1]);
		char buf[1024] = {0};
		lseek(fd, 0, SEEK_SET);
		read(fd, buf, sizeof(buf)-1);
		printf("【%d】:%s\n", getppid(), buf);
		close(fd);
		exit(0);
	}
	close(sv[1]);
	int fd = open("svfile", O_RDWR|O_CREAT, 0644);
	char buf[1024] = "你好，子进程";
	write(fd, buf, strlen(buf));

	send_fd(sv[0], fd);
	close(fd);
	wait(NULL);

	return 0;
}
