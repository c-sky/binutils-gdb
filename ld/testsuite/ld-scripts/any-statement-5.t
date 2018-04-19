MEMORY
{
  ram : ORIGIN = 0x100000, LENGTH = 0x10
}

SECTIONS
{
  tt :
  {
        .ANY
  } >ram
}

