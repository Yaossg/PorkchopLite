#include <stdio.h>
#include <stdlib.h>

typedef void pc_none;
#define pc_never _Noreturn void
typedef _Bool pc_bool;
typedef long pc_int;
typedef double pc_float;


pc_none printint(pc_int value) {
    printf("%ld\n", value);
}

pc_none printfloat(pc_float value) {
    printf("%.6f\n", value);
}

pc_none print_int_array(pc_int* array, pc_int size) {
    for (int i = 0; i < size; ++i) {
        printf("%ld ", array[i]);
    }
    printf("\n");
}

pc_never exit(int);

pc_int* alloc_int_array(pc_int size) {
    return aligned_alloc(8, sizeof(pc_int) * size);
}

pc_none dealloc_int_array(pc_int* array) {
    free(array);
}