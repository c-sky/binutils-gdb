#source: any-statement-2.s
#ld: --any_sort_order=cmdline --any_contingency=50 -T any-statement-7c.t
#readelf: -S --wide

#...
  \[[ 0-9]+\] mem[ \t]+PROGBITS[ \t]+00000000[ \t0-9]+000030[ \t0-9]+WAX?.*
#...
  \[[ 0-9]+\] mem1[ \t]+PROGBITS[ \t]+00000100[ \t0-9]+000040[ \t0-9]+A?.*
#pass
