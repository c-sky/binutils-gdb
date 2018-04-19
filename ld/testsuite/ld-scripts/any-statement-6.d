#source: any-statement-6.s
#ld: -T any-statement-6.t --gc-sections
#readelf: -S --wide

#...
  \[[ 0-9]+\] MEM[ \t]+PROGBITS[ \t]+00100000[ \t0-9]+000014[ \t0-9]+AX?.*
#pass
