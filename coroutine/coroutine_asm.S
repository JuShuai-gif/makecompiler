.text
.global __asm_co_task_run, __asm_co_task_yield, __save_stack
.global scf_async, __scf_async

.align 8
__asm_co_task_run:
# rdi, task_rsp addr
# rsi, stack_data
# rdx, task_rip
# rcx, stack_len
# r8,  task_rsp0 addr 
# r9,  n_floats

	push %rbp
	push %rdi
	push %rsi

	push %rbx
	push %rcx
	push %rdx

	push %r12
	push %r13
	push %r14
	push %r15

	mov (%r8), %rax
	mov  %rsp, (%r8)
	mov  %rdx, %rbx

	mov  %rsp, %rbp

_fill:
	sub  %rcx, %rsp
	mov  %rsp, %rdi

	push %rcx
	test $0x7, %rcx
	jne 1f

	shr $3, %rcx
	cld
	rep movsq
	jmp 3f
1:
	test $0x3, %rcx
	jne 2f

	shr $2, %rcx
	cld
	rep movsl
	jmp 3f
2:
	cld
	rep movsb
3:
	pop  %rcx

	test %rax, %rax
	jne  _jmp

	movq %r9, %rax

	pop %rdi
	pop %rsi
	pop %rdx
	pop %rcx
	pop %r8
	pop %r9

	movsd   (%rsp), %xmm0
	movsd  8(%rsp), %xmm1
	movsd 16(%rsp), %xmm2
	movsd 24(%rsp), %xmm3
	movsd 32(%rsp), %xmm4
	movsd 40(%rsp), %xmm5
	movsd 48(%rsp), %xmm6
	movsd 56(%rsp), %xmm7

	addq $64, %rsp

	call *%rbx
	jmp  _reset

_jmp:
	jmp  *%rbx

_reset:
	mov %rbp, %rsp
	mov %rsp, %rax

__jmp_ret:
	pop %r15
	pop %r14
	pop %r13
	pop %r12

	pop %rdx
	pop %rcx
	pop %rbx

	pop %rsi
	pop %rdi
	pop %rbp
	ret

.align 8
__asm_co_task_yield:
# rdi, task pointer
# rsi, task_rip  addr
# rdx, task_rsp  addr
# rcx, task_rsp0

	push %rbp
	push %rdi
	push %rsi

	push %rbx
	push %rcx
	push %rdx

	push %r12
	push %r13
	push %r14
	push %r15

	jmp  2f
1:
	pop  %rax          # rax == __jmp_yield
	movq %rax, (%rsi)
	mov  %rsp, (%rdx)

	push %rsp
	push %rcx

	call __save_stack@PLT

	pop  %rcx
	pop  %rsp
	mov  %rsp, %rax
	mov  %rcx, %rsp

	jmp  __jmp_ret

2:
	call 1b
__jmp_yield:
	pop %r15
	pop %r14
	pop %r13
	pop %r12

	pop %rdx
	pop %rcx
	pop %rbx

	pop %rsi
	pop %rdi
	pop %rbp

	mov %rsp, %rax
	ret

.align 8
scf_async:
# rdi, funcptr
# rsi, fmt
# rdx, d0
# rcx, d1
# r8,  d2
# r9,  d3
# xmm0-xmm7, f0-f7

# 24(rsp), ...
# 16(rsp), stack arg1
#  8(rsp), stack arg0  <--- rax
#  0(rsp), ret addr

	movq %rsp, %rax
	addq $8,   %rax

	push %rax
	call __scf_async@PLT

	addq $8, %rsp
	ret
