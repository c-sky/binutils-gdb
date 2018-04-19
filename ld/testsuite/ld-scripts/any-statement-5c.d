#source: any-statement-1.s
#ld: --any_sort_order=cmdline -T any-statement-5.t
# error: \A[^ \n]*?ld[^:\n]*?: [^\n]*?there is no space to put .* section `.data' in specified region.\Z
