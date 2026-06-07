cp ../../build/header_only/cup.h .
gcc -x c cup.h -DMAIN_ENTRY -o cup -D_GNU_SOURCE -fms-extensions
./cup