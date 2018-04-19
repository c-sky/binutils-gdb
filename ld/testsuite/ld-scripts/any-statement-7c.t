MEMORY
{
    mem : ORIGIN = 0X0 , LENGTH = 0X60
    mem1 : ORIGIN = 0X100 , LENGTH = 0X80
}

SECTIONS
{
    mem : {
        .ANY
    } > mem

    mem1 : {
        .ANY
    } > mem1
}
