#source: dummy.s
#ld: -u foo --gc-sections tmpdir/libpr20306.so
#xfail: "csky-*-elf"
#error: .* generated: undefined reference to `foo'
