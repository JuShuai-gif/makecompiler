.text
.global _start, main

_start:
	mov  %rsp, %rsi
	add  $8,   %rsi   # argv
	mov  %rsi, %rdx
1:
	mov  (%rdx), %rdi
	add  $8,    %rdx  # envp
	test %rdi,  %rdi
	jnz  1b

	mov  (%rsp), %rdi # argc

	call main
	mov  %rax, %rdi
	mov  $60,  %rax
	syscall
.fill 5, 1, 0
