MEMORY
{
  ram : ORIGIN = 0x100000, LENGTH = 0x100
  rom : ORIGIN = 0x200000, LENGTH = 0x100
}

SECTIONS
{
  rom :
  {
    INPUT_SECTION_FLAGS (!SHF_WRITE).ANY
  } >rom

  ram :
  {
    .ANY
  } > ram
}

