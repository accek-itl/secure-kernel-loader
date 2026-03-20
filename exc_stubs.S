#include <defs.h>

#ifdef DEBUG

	.text

/*
 * Exception stub macros. Each stub is padded to 16 bytes so the IDT
 * setup can compute handler addresses as exc_stubs + vector * 16.
 */
#ifdef __x86_64__

.macro exc_stub_noerr vec
	.align 16
	pushq $0
	pushq $\vec
	jmp exc_common
.endm

.macro exc_stub_err vec
	.align 16
	pushq $\vec
	jmp exc_common
.endm

#else /* 32-bit */

.macro exc_stub_noerr vec
	.align 16
	pushl $0
	pushl $\vec
	jmp exc_common
.endm

.macro exc_stub_err vec
	.align 16
	pushl $\vec
	jmp exc_common
.endm

#endif

	.align 16
GLOBAL(exc_stubs)
	exc_stub_noerr 0	/* #DE */
	exc_stub_noerr 1	/* #DB */
	exc_stub_noerr 2	/* NMI */
	exc_stub_noerr 3	/* #BP */
	exc_stub_noerr 4	/* #OF */
	exc_stub_noerr 5	/* #BR */
	exc_stub_noerr 6	/* #UD */
	exc_stub_noerr 7	/* #NM */
	exc_stub_err   8	/* #DF */
	exc_stub_noerr 9	/* CSO */
	exc_stub_err   10	/* #TS */
	exc_stub_err   11	/* #NP */
	exc_stub_err   12	/* #SS */
	exc_stub_err   13	/* #GP */
	exc_stub_err   14	/* #PF */
	exc_stub_noerr 15
	exc_stub_noerr 16	/* #MF */
	exc_stub_err   17	/* #AC */
	exc_stub_noerr 18	/* #MC */
	exc_stub_noerr 19	/* #XM */
	exc_stub_noerr 20	/* #VE */
	exc_stub_err   21	/* #CP */
	exc_stub_noerr 22
	exc_stub_noerr 23
	exc_stub_noerr 24
	exc_stub_noerr 25
	exc_stub_noerr 26
	exc_stub_noerr 27
	exc_stub_noerr 28	/* #HV */
	exc_stub_err   29	/* #VC */
	exc_stub_err   30	/* #SX */
	exc_stub_noerr 31
GLOBAL(exc_stubs_end)

#ifdef __x86_64__

exc_common:
	pushq %rax
	pushq %rcx
	pushq %rdx
	pushq %rbx
	pushq %rbp
	pushq %rsi
	pushq %rdi
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	movq  %rsp, %rdi	/* arg1 (SysV ABI) = frame pointer */
	call  idt_exc_handler
1:	hlt
	jmp   1b
ENDFUNC(exc_common)

#else /* 32-bit */

exc_common:
	pushal
	mov	%esp, %eax	/* arg1 (regparm=3) = frame pointer */
	call	idt_exc_handler
1:	hlt
	jmp	1b
ENDFUNC(exc_common)

#endif

#endif /* DEBUG */

	.section .note.GNU-stack
