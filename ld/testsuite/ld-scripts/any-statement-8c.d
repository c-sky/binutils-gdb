#source: any-statement-1.s
#ld: --any_sort_order=cmdline -T any-statement-8.t
#readelf: -s

Symbol table '.symtab' contains .* entries:
#...
    .*: 00000021     0 NOTYPE  GLOBAL DEFAULT    1 test_assign
#...
