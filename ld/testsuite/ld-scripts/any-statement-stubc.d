#source: any-statement-stub.s
#ld: --any_sort_order=cmdline -T any-statement-stub.t
#readelf: -S --wide

#...
  \[[ 0-9]+\] mem[ \t]+PROGBITS[ \t]+00000000[ \t0-9]+000030[ \t0-9]+WAX?.*
#...
  \[[ 0-9]+\] mem1[ \t]+PROGBITS[ \t]+05000000[ \t0-9]+4000002[ \t0-9]+AX?.*
#pass
