MEMORY
{
  ram : ORIGIN = 0x100000, LENGTH = 0x100
  rom : ORIGIN = 0x200000, LENGTH = 0x100
}

SECTIONS
{
  rom : SUBALIGN(0x20)
  {
    .ANY(.text* .rodata*)
  } >rom

  ram :
  {
    .ANY(.data*)
  } > ram
}

