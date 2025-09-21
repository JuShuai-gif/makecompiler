#include"coroutine.h"

#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>

#include<sys/epoll.h>
#include<sys/time.h>
#include<fcntl.h>

static int _async_test()
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		loge("\n");
		return -1;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(2000);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	int ret = __async_connect(fd, (struct sockaddr*)&addr, sizeof(addr), 1000);
	if (ret < 0) {
		loge("\n");
		return -1;
	}

	ret = __async_write(fd, "hello\n", 7);
	if (ret < 0) {
		loge("\n");
		return -1;
	}

	logi("async test ok, ret: %d\n", ret);
	return 0;
}

int main()
{
	co_thread_t* thread = NULL;
	co_task_t*   task   = NULL;

	int ret = co_thread_open(&thread);
	if (ret < 0) {
		loge("\n");
		return -1;
	}

	uintptr_t rsp[] = {5, 6, 0.1, 7};

	*(double*)&rsp[2] = 0.123;

// interger: rdi rsi rdx rcx r8 r9, float / double: xmm0-xmm7, stack rsp: ...
	ret = __async((uintptr_t)_async_test, "dddd ffff ffff", 1, 2, 3, 4, rsp, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7);
	if (ret < 0) {
		loge("\n");
		return -1;
	}

	logi("thread: %p\n", thread);

	ret = co_thread_run(thread);
	if (ret < 0) {
		loge("\n");
		return -1;
	}

	return 0;
}
