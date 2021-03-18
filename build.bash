make clean && make -C aquahash clean
make -j 4 config=avx2 OPTTARGET=core-avx2
