#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

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

pc_int thread_create(pc_none *(*callback)(pc_none *), pc_none* arg) {
    pthread_t thread;
    pthread_create(&thread, NULL, callback, arg);
    return thread;
}

pc_none* thread_join(pc_int thread) {
    pc_none* ret;
    pthread_join(thread, &ret);
    return ret;
}

pc_int thread_self() {
    return pthread_self();
}

pc_float pc_time() {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

typedef struct {
    pc_none* data;
    pc_int size;
    pc_int offset;
    pc_int length;
    pc_none *(*proj)(pc_none *);
    pc_none *(*acc)(pc_none *, pc_none *);
} parallel_reduce_task;

static pc_none* parallel_reduce_routine(pc_none* task_) {
    parallel_reduce_task* task = task_;
    char* base = (char*)task->data + task->offset * task->size;
    pc_none* initial = task->proj(base);
    for (pc_int i = 1; i < task->length; ++i) {
        initial = task->acc(initial, task->proj(base + i * task->size));
    }
    return initial;
}

pc_none* parallel_reduce(pc_none* data, pc_int size, pc_int length,
    pc_none *(*proj)(pc_none *), pc_none *(*acc)(pc_none *, pc_none *)) {
    if (length == 0) return NULL;
    const pc_int CPU_COUNT = 16;
    pc_int task_count = length < CPU_COUNT ? length : CPU_COUNT;
    pthread_t* threads = malloc(sizeof(pc_int) * task_count);
    parallel_reduce_task* tasks = malloc(sizeof(parallel_reduce_task) * task_count);
    for (pc_int i = 0; i < task_count; ++i) {
        tasks[i].data = data;
        tasks[i].size = size;
        tasks[i].offset = i * length / task_count;
        tasks[i].length = (i + 1) * length / task_count - i * length / task_count;
        tasks[i].proj = proj;
        tasks[i].acc = acc;
        pthread_create(threads + i, NULL, parallel_reduce_routine, tasks + i);
    }
    pc_none* initial;
    pthread_join(threads[0], &initial);
    for (pc_int i = 1; i < task_count; ++i) {
        pc_none* result;
        pthread_join(threads[i], &result);
        initial = acc(initial, result);
    }
    free(tasks);
    free(threads);
    return initial;
}



typedef struct {
    pc_none* data;
    pc_int offset;
    pc_int length;
    pc_none *(*thread)(pc_none *data, pc_int offset, pc_int length);
} parallel_for_task;

static pc_none* parallel_for_routine(pc_none* task_) {
    parallel_for_task* task = task_;
    return task->thread(task->data, task->offset, task->length);
}

pc_none** parallel_for(pc_none* data, pc_int length, pc_none *(*thread)(pc_none *, pc_int, pc_int)) {
    if (length == 0) return NULL;
    const pc_int CPU_COUNT = 16;
    pc_int task_count = length < CPU_COUNT ? length : CPU_COUNT;
    pthread_t* threads = malloc(sizeof(pc_int) * task_count);
    parallel_for_task* tasks = malloc(sizeof(parallel_reduce_task) * task_count);
    pc_none** results = malloc(sizeof(pc_none*) * task_count);
    for (pc_int i = 0; i < task_count; ++i) {
        tasks[i].data = data;
        tasks[i].offset = i * length / task_count;
        tasks[i].length = (i + 1) * length / task_count - i * length / task_count;
        tasks[i].thread = thread;
        pthread_create(threads + i, NULL, parallel_for_routine, tasks + i);
    }
    for (pc_int i = 0; i < task_count; ++i) {
        pc_none* result;
        pthread_join(threads[i], &result);
        results[i] = result;
    }
    free(tasks);
    free(threads);
    return results;
}