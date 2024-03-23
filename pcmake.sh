input=()
output="a.out"
interpret=0
verbose=0
while [ $OPTIND -le "$#" ]
do
    if getopts go:iv option
    then
        case $option
        in
            g) g="-g";;
            o) output=${OPTARG};;
            i) interpret=1;;
            v) verbose=1;;
        esac
    else
        input+=("${!OPTIND}")
        ((OPTIND++))
    fi
done

cnt=0
make PorkchopLite -j$(nproc)
for file in "${input[@]}"
do
  if [[ $file == *.c ]]
  then
    clang -emit-llvm -S $file $g -o "$((cnt++)).ll"
  elif [[ $file == *.pc ]]
  then
    ./PorkchopLite $file -o "$((cnt++)).ll" -l $g
  fi
done

ir=()
for i in $(seq 0 $((cnt-1)))
do
  ir+=("${i}.ll")
done

llvm-link -opaque-pointers "${ir[@]}" -S -o out.ll

if [ $interpret -eq 1 ]
then
  lli -opaque-pointers out.ll
  echo "returned with code" $?
else
  llc -opaque-pointers out.ll -filetype=obj
  gcc -no-pie out.o -o $output
fi

if [ $verbose -eq 0 ]
then
  rm *.ll
  rm out.o
fi
