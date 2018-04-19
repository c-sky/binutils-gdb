MEMORY
{
  ram (rwx) : ORIGIN = 0x100000, LENGTH = 144M
}

SECTIONS
{
  .text :
  {
        . = 0x1;
        .ANY
        test_assign = .;
  } >ram
}

