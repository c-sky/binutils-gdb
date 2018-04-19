#source: any-statement-stub.s
#ld: -T any-statement-stub.t
#readelf: -S --wide

#...
  \[[ 0-9]+\] mem[ \t]+PROGBITS[ \t]+00000000[ \t0-9]+000020[ \t0-9]+WAX?.*
#...
  \[[ 0-9]+\] mem1[ \t]+PROGBITS[ \t]+05000000[ \t0-9]+4000012[ \t0-9]+AX?.*
#pass
