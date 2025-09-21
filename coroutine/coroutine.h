#ifndef COROUTINE_H
#define COROUTINE_H

#include"utils_list.h"
#include"rbtree.h"

#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>

#include<sys/epoll.h>
#include<sys/time.h>
#include<fcntl.h>

typedef struct co_task_s    co_task_t;
typedef struct co_thread_s  co_thread_t;

typedef struct co_buf_s     co_buf_t;

extern         co_thread_t* __co_thread;

struct co_buf_s
{
	co_buf_t*      next;

	size_t             len;
	size_t             pos;
	uint8_t            data[0];
};

#define CO_ERROR   -1
#define CO_OK       0
#define CO_CONTINUE 1

struct co_task_s
{
	rbtree_node_t  timer;
	int64_t            time;

	list_t         list;

	co_thread_t*   thread;

	uintptr_t          rip;
	uintptr_t          rsp;
	uintptr_t          rsp0;

	uintptr_t*         stack_data;
	intptr_t           stack_len;
	intptr_t           stack_capacity;

	int                n_floats;
	int                err;

	uint32_t           events;
	int                fd;

	co_buf_t*      read_bufs;
};

struct co_thread_s
{
	int                epfd;

	rbtree_t       timers;

	list_t         tasks;
	int                n_tasks;

	co_task_t*     current;

	uint32_t           exit_flag;
};

void __async_exit();
void __async_msleep(int64_t msec);
int  __async_read (int fd, void* buf, size_t count, int64_t msec);
int  __async_write(int fd, void* buf, size_t count);
int  __async_connect(int fd, const struct sockaddr *addr, socklen_t addrlen, int64_t msec);
int  __async_loop();


int  co_thread_open (co_thread_t** pthread);
int  co_thread_close(co_thread_t*  thread);

void co_thread_add_task(co_thread_t* thread, co_task_t* task);

int  co_thread_run(co_thread_t*  thread);

int  co_task_alloc(co_task_t** ptask, uintptr_t funcptr, const char* fmt, uintptr_t rdx, uintptr_t rcx, uintptr_t r8, uintptr_t r9, uintptr_t* rsp,
		double xmm0, double xmm1, double xmm2, double xmm3, double xmm4, double xmm5, double xmm6, double xmm7);

void co_task_free(co_task_t* task);

int  __async(uintptr_t funcptr, const char* fmt, uintptr_t rdx, uintptr_t rcx, uintptr_t r8, uintptr_t r9, uintptr_t* rsp,
		double xmm0, double xmm1, double xmm2, double xmm3, double xmm4, double xmm5, double xmm6, double xmm7);

#endif
