make Porkchop -j8 &&
clang -emit-llvm -S ../lib/lib.c -o lib.ll &&
./Porkchop main.pc -o main.ll -l &&
llvm-link -opaque-pointers main.ll lib.ll -S -o out.ll &&
lli -opaque-pointers out.ll
echo "returned with code" $?
