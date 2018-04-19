#source: any-statement-2.s
#ld: -T any-statement-3.t
#readelf: -S --wide

#...
  \[[ 0-9]+\] rom[ \t]+PROGBITS[ \t]+00200000[ \t0-9]+000040[ \t0-9]+AX?.*
#...
  \[[ 0-9]+\] ram[ \t]+PROGBITS[ \t]+00100000[ \t0-9]+000030[ \t0-9]+WA?.*
#pass
