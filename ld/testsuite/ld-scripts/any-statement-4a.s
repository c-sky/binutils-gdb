    .text
    .space 0x100

    .data
    .space 0x200

    .section .rodata
    .space 0x400

    .section .bss ,"aw",@nobits
    .space 0x800
