#include"coroutine.h"

uintptr_t __asm_co_task_run(uintptr_t* task_rsp, void* stack_data,
		uintptr_t task_rip, intptr_t stack_len, uintptr_t* task_rsp0, int n_floats);

int __co_task_run(co_task_t* task)
{
	task->thread->current = task;

	task->err = 0;

	uintptr_t rsp1 = 0;
	logw("task %p, rsp0: %#lx, rsp1: %#lx\n", task, task->rsp0, rsp1);

	rsp1 = __asm_co_task_run(&task->rsp, task->stack_data, task->rip,
			task->stack_len, &task->rsp0, task->n_floats);

	if (task->err) {
		loge("task %p error: %d, rsp0: %#lx, rsp1: %#lx, task->time: %ld\n",
				task, task->err, task->rsp0, rsp1, task->time);
		return task->err;

	} else if (task->rsp0 == rsp1)
		return CO_OK;

	logi("task %p running, rsp0: %#lx, rsp1: %#lx\n", task, task->rsp0, rsp1);
	return CO_CONTINUE;
}
