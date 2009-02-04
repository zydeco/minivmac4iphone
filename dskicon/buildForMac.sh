#!/bin/sh

for libdir in libhfs libmfs libres
do
    cd $libdir
    make clean
    make
    cd ..
done

make clean
make