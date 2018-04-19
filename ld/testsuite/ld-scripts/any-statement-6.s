.global main
.global func
    .section .text.1,"ax",@progbits
main:
    bsr func

    
    .section .text.2,"ax",@progbits
func:
    .space 16

    .section .text.3,"ax",@progbits
    .space 16
