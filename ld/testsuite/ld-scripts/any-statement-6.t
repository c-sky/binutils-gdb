ENTRY(main)

MEMORY
{
  MEM : ORIGIN = 0x100000, LENGTH = 0x100
}

SECTIONS
{
  MEM :
  {
        .ANY
  } >MEM
}

