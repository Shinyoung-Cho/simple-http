# simple-http

Even simpler http web server that is implemented in c/c++, using only the native libraries.

Can be used on linux environment.

Almost none of the HTTP spec is implemented, but might be useful under certain conditions.

Compile with -std=c++14, link with -lpthread option.



handles 20,000+ req / sec
tested on Intel(R) Xeon(R) CPU E5-2666 v3 @ 2.90GHz, 8cpu
