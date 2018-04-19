#source: any-statement-4a.s
#source: any-statement-4a.s
#source: any-statement-4a.s
#ld: -T any-statement-4.t
#readelf: -Sl --wide

#...
  \[[ 0-9]+\] ER_IROM1[ \t]+PROGBITS[ \t]+08000000[ \t0-9]+000f00[ \t0-9]+AX?.*
  \[[ 0-9]+\] RW_IRAM1[ \t]+PROGBITS[ \t]+02000000[ \t0-9]+000200[ \t0-9]+WA?.*
  \[[ 0-9]+\] BSS_RW_IRAM1[ \t]+PROGBITS[ \t]+02000200[ \t0-9]+000000[ \t0-9]+W?.*
  \[[ 0-9]+\] RW_IRAM2[ \t]+PROGBITS[ \t]+02005000[ \t0-9]+000400[ \t0-9]+WA?.*
  \[[ 0-9]+\] BSS_RW_IRAM2[ \t]+NOBITS[ \t]+02005400[ \t0-9]+001800[ \t0-9]+WA?.*
#...
Program Headers:
 +Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg +Align
 +LOAD +0x[0-9a-f]+ 0x08000000 0x08000000 0x00f00 0x00f00 +R E +0x[0-9a-f]+
 +LOAD +0x[0-9a-f]+ 0x02000000 0x08000f00 0x00200 0x00200 +RW +0x[0-9a-f]+
 +LOAD +0x[0-9a-f]+ 0x02005000 0x09000000 0x00400 0x01c00 +RW +0x[0-9a-f]+

 Section to Segment mapping:
 +Segment Sections\.\.\.
 +00[ \t]+ER_IROM1 +
 +01[ \t]+RW_IRAM1 +
 +02[ \t]+RW_IRAM2 +BSS_RW_IRAM2 +
#pass
