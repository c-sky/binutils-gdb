.global func
	.type	func, @function

	.text
	bsr func
	.section .text1,"ax",@progbits
	.space 64*1024*1024
func:
	nop
	.section .t1,"ax",@progbits
	.space 16
	.section .t2,"ax",@progbits
	.space 16
