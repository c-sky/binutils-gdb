#source: pr13195.s
#ld: -shared -version-script pr13195.t
#nm: -D
#target: *-*-linux* *-*-gnu* arm*-*-uclinuxfdpiceabi
#xfail: h8300-*-* hppa64-*-*
# h8300 doesn't support -shared, and hppa64 creates .foo

#..
0+ A VERS_2.0
[0-9a-f]+ T foo@@VERS_2.0
#pass