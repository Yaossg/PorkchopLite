# build
mkdir -p ../build
cd ../build
cmake ..
make PorkchopLite -j$(nproc)
cd ../test
clang -emit-llvm -S ../lib/lib.c -o lib.ll

# execute
for x in *.pc; do
  echo "testing source code" $x
  ../build/PorkchopLite $x -o $x.ll -l &&
  llvm-link -opaque-pointers $x.ll lib.ll -S -o $x.out.ll &&
  lli -opaque-pointers $x.out.ll > $x.o
  echo "returned with code" $? >> $x.o
  echo "succeeded to execute" $x
done

# cleanup
for x in *.pc.out.ll; do
  rm $x
done
rm lib.ll