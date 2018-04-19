#source: any-statement-2.s
#ld: -T any-statement-2.t
#readelf: -S --wide

#...
  \[[ 0-9]+\] rom[ \t]+PROGBITS[ \t]+00200000[ \t0-9]+000050[ \t0-9]+AX?.*
#...
  \[[ 0-9]+\] ram[ \t]+PROGBITS[ \t]+00100000[ \t0-9]+000020[ \t0-9]+WA?.*
#pass
