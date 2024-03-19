#include <stdio.h>
#include <stdlib.h>

typedef void pc_none;
#define pc_never _Noreturn void
typedef _Bool pc_bool;
typedef long pc_int;
typedef double pc_float;

pc_none* alloc(pc_int size) {
    return malloc(size);
}

pc_none dealloc(pc_none* array) {
    free(array);
}

pc_none printint(pc_int value) {
    printf("%ld\n", value);
}

pc_none printfloat(pc_float value) {
    printf("%f\n", value);
}

pc_none print_int_array(pc_int* array, pc_int size) {
    for (int i = 0; i < size; ++i) {
        printf("%ld ", array[i]);
    }
    printf("\n");
}

pc_never exit(int);