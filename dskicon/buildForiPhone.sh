#!/bin/sh

for libdir in libhfs libmfs libres
do
    cd $libdir
    make clean -f Makefile.iPhone
    make -f Makefile.iPhone
    cd ..
done

make clean -f Makefile.iPhone
make -f Makefile.iPhone