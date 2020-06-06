#!/bin/sh -ex
SOURCE=$(dirname $0)
MACHINE=$(gcc -dumpmachine)
mkdir -p .modules-B4E4FE1B-$MACHINE-kissmalloc_src
mkdir -p .modules-6C99ABEF-$MACHINE-tools_bench
mkdir -p .modules-04A1B017-$MACHINE-tools_bench_libc
mkdir -p .modules-07136E57-$MACHINE-tools_bench_threads
mkdir -p .modules-0F2EC1CC-$MACHINE-tools_bench_threads_libc
mkdir -p .modules-CC779790-$MACHINE-tools_bench_std_list
mkdir -p .modules-C8A5C153-$MACHINE-tools_bench_std_list_libc
gcc -c -o .modules-B4E4FE1B-$MACHINE-kissmalloc_src/kissmalloc.o -DNDEBUG -O2 -fPIC -Wall -pthread -pipe -D_FILE_OFFSET_BITS=64 -DCCBUILD_BUNDLE_VERSION=0.1.0 -DKISSMALLOC_OVERLOAD_LIBC $SOURCE/src/kissmalloc.c &
g++ -c -o .modules-B4E4FE1B-$MACHINE-kissmalloc_src/kissmalloc_new.o -DNDEBUG -O2 -fPIC -Wall -pthread -pipe -std=c++11 -D_FILE_OFFSET_BITS=64 -DCCBUILD_BUNDLE_VERSION=0.1.0 -DKISSMALLOC_OVERLOAD_LIBC $SOURCE/src/kissmalloc_new.cc &
wait
g++ -o libkissmalloc.so.0.1.0 -shared -pthread -Wl,-soname,libkissmalloc.so.0 .modules-B4E4FE1B-$MACHINE-kissmalloc_src/kissmalloc.o .modules-B4E4FE1B-$MACHINE-kissmalloc_src/kissmalloc_new.o -L. -Wl,--enable-new-dtags,-rpath='$ORIGIN',-rpath='$ORIGIN'/../lib,-rpath-link=$PWD
rm -f libkissmalloc.so.0.1
rm -f libkissmalloc.so.0
rm -f libkissmalloc.so
ln -sf libkissmalloc.so.0.1.0 libkissmalloc.so.0.1
ln -sf libkissmalloc.so.0.1.0 libkissmalloc.so.0
ln -sf libkissmalloc.so.0.1.0 libkissmalloc.so
gcc -c -o .modules-6C99ABEF-$MACHINE-tools_bench/main.o -DNDEBUG -O2 -fPIC -Wall -pthread -pipe -D_FILE_OFFSET_BITS=64 -DCCBUILD_BUNDLE_VERSION=0.1.0 -DKISSMALLOC_OVERLOAD_LIBC -I$SOURCE/src $SOURCE/tools/bench/main.c &
wait
gcc -o kissbench -pthread .modules-6C99ABEF-$MACHINE-tools_bench/main.o -L. -lkissmalloc -Wl,--enable-new-dtags,-rpath='$ORIGIN',-rpath='$ORIGIN'/../lib,-rpath-link=$PWD
gcc -c -o .modules-04A1B017-$MACHINE-tools_bench_libc/main.o -DNDEBUG -O2 -fPIC -Wall -pthread -pipe -D_FILE_OFFSET_BITS=64 -DCCBUILD_BUNDLE_VERSION=0.1.0 $SOURCE/tools/bench_libc/main.c &
wait
gcc -o kissbench_libc -pthread .modules-04A1B017-$MACHINE-tools_bench_libc/main.o -L. -Wl,--enable-new-dtags,-rpath='$ORIGIN',-rpath='$ORIGIN'/../lib,-rpath-link=$PWD
gcc -c -o .modules-07136E57-$MACHINE-tools_bench_threads/main.o -DNDEBUG -O2 -fPIC -Wall -pthread -pipe -D_FILE_OFFSET_BITS=64 -DCCBUILD_BUNDLE_VERSION=0.1.0 -DKISSMALLOC_OVERLOAD_LIBC -I$SOURCE/src $SOURCE/tools/bench_threads/main.c &
wait
gcc -o kissbench_threads -pthread .modules-07136E57-$MACHINE-tools_bench_threads/main.o -L. -lkissmalloc -Wl,--enable-new-dtags,-rpath='$ORIGIN',-rpath='$ORIGIN'/../lib,-rpath-link=$PWD
gcc -c -o .modules-0F2EC1CC-$MACHINE-tools_bench_threads_libc/main.o -DNDEBUG -O2 -fPIC -Wall -pthread -pipe -D_FILE_OFFSET_BITS=64 -DCCBUILD_BUNDLE_VERSION=0.1.0 $SOURCE/tools/bench_threads_libc/main.c &
wait
gcc -o kissbench_threads_libc -pthread .modules-0F2EC1CC-$MACHINE-tools_bench_threads_libc/main.o -L. -Wl,--enable-new-dtags,-rpath='$ORIGIN',-rpath='$ORIGIN'/../lib,-rpath-link=$PWD
g++ -c -o .modules-CC779790-$MACHINE-tools_bench_std_list/main.o -DNDEBUG -O2 -fPIC -Wall -pthread -pipe -std=c++11 -D_FILE_OFFSET_BITS=64 -DCCBUILD_BUNDLE_VERSION=0.1.0 -DKISSMALLOC_OVERLOAD_LIBC -I$SOURCE/src $SOURCE/tools/bench_std_list/main.cc &
wait
g++ -o kissbench_std_list -pthread .modules-CC779790-$MACHINE-tools_bench_std_list/main.o -L. -lkissmalloc -Wl,--enable-new-dtags,-rpath='$ORIGIN',-rpath='$ORIGIN'/../lib,-rpath-link=$PWD
g++ -c -o .modules-C8A5C153-$MACHINE-tools_bench_std_list_libc/main.o -DNDEBUG -O2 -fPIC -Wall -pthread -pipe -std=c++11 -D_FILE_OFFSET_BITS=64 -DCCBUILD_BUNDLE_VERSION=0.1.0 $SOURCE/tools/bench_std_list_libc/main.cc &
wait
g++ -o kissbench_std_list_libc -pthread .modules-C8A5C153-$MACHINE-tools_bench_std_list_libc/main.o -L. -Wl,--enable-new-dtags,-rpath='$ORIGIN',-rpath='$ORIGIN'/../lib,-rpath-link=$PWD
