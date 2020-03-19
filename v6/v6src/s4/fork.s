/ C library -- fork

/ pid = fork();
/
/ pid == 0 in child process; pid == -1 means error return
/ in child, parents id is in par_uid if needed

.globl	_fork, cerror, _par_uid   # 表示注释，unix v6 的C编译器将C中的函数转换为起始位置带有下划线"_"的标签

_fork: # 相当于C语言中的fork()函数
	mov	r5,-(sp) #
	mov	sp,r5
	sys	fork  # sys指令是用于执行系统调用的汇编指令，它的参数是执行哪个系统调用的整数
		br 1f
	bec	2f
	jmp	cerror
1:
	mov	r0,_par_uid
	clr	r0
2:
	mov	(sp)+,r5
	rts	pc
.bss
_par_uid: .=.+2
