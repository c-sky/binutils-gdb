MEMORY
{
    LR_IROM1 : ORIGIN = 0X8000000, LENGTH = 0X10000
    LR_IROM2 : ORIGIN = 0X9000000, LENGTH = 0X5000
    ER_IROM1 : ORIGIN = 0X8000000, LENGTH = 0X5000
    RW_IRAM1 : ORIGIN = 0X2000000, LENGTH = 0xe00
    RW_IRAM2 : ORIGIN = 0X2005000, LENGTH = 0X5000
}

SECTIONS
{
    ER_IROM1 : {
        .ANY(.text* .rodata*)
    } >ER_IROM1 AT>LR_IROM1

    RW_IRAM1 : {
        *dump0.o(.data*)
        .ANY(.data*)
    } >RW_IRAM1 AT>LR_IROM1

    BSS_RW_IRAM1 : {
        .ANY(.bss*)
    } >RW_IRAM1 AT>LR_IROM1

    RW_IRAM2 : {
        *dump1.o(.data*)
        .ANY(.data*)
    } >RW_IRAM2 AT>LR_IROM2

    BSS_RW_IRAM2 : {
        .ANY(.bss*)
    } >RW_IRAM2 AT>LR_IROM2
}
