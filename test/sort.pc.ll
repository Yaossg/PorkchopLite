declare void @printint(i64 %0)
declare void @printfloat(double %0)
declare void @print_int_array(ptr %0, i64 %1)
declare ptr @parallel_reduce(ptr %0, i64 %1, i64 %2, ptr %3, ptr %4)
declare ptr @alloc(i64 %0)
declare void @dealloc(ptr %0)
declare ptr @parallel_for(ptr %0, i64 %1, ptr %2)
declare i64 @thread_create(ptr %0, ptr %1)
declare ptr @thread_join(i64 %0)
declare i64 @thread_self()
declare double @pc_time()
declare void @exit(i64 %0)
define void @swap(ptr %0, ptr %1) {
L0:
    %2 = alloca ptr
    store ptr %0, ptr %2
    %3 = alloca ptr
    store ptr %1, ptr %3
    %4 = alloca i64
    %5 = load ptr, ptr %2
    %6 = load i64, ptr %5
    store i64 %6, ptr %4
    %7 = load ptr, ptr %3
    %8 = load i64, ptr %7
    %9 = load ptr, ptr %2
    store i64 %8, ptr %9
    %10 = load i64, ptr %4
    %11 = load ptr, ptr %3
    store i64 %10, ptr %11
    ret void
}
define ptr @min_element(ptr %0, ptr %1, ptr %2) {
L0:
    %3 = alloca ptr
    store ptr %0, ptr %3
    %4 = alloca ptr
    store ptr %1, ptr %4
    %5 = alloca ptr
    store ptr %2, ptr %5
    %6 = alloca ptr
    %7 = load ptr, ptr %3
    store ptr %7, ptr %6
    br label %L1
L1:
    %8 = load ptr, ptr %3
    %9 = load ptr, ptr %4
    %10 = icmp ne ptr %8, %9
    br i1 %10, label %L2, label %L3
L2:
    %11 = load ptr, ptr %5
    %12 = load ptr, ptr %3
    %13 = load i64, ptr %12
    %14 = load ptr, ptr %6
    %15 = load i64, ptr %14
    %16 = call i1 %11(i64 %13, i64 %15)
    br i1 %16, label %L4, label %L5
L4:
    %17 = load ptr, ptr %3
    store ptr %17, ptr %6
    br label %L6
L5:
    br label %L6
L6:
    %18 = load ptr, ptr %3
    %19 = getelementptr inbounds ptr, ptr %18, i64 1
    store ptr %19, ptr %3
    br label %L1
L3:
    %20 = load ptr, ptr %6
    ret ptr %20
}
define void @sort(ptr %0, ptr %1, ptr %2) {
L0:
    %3 = alloca ptr
    store ptr %0, ptr %3
    %4 = alloca ptr
    store ptr %1, ptr %4
    %5 = alloca ptr
    store ptr %2, ptr %5
    br label %L1
L1:
    %6 = load ptr, ptr %3
    %7 = load ptr, ptr %4
    %8 = icmp ne ptr %6, %7
    br i1 %8, label %L2, label %L3
L2:
    %9 = load ptr, ptr %3
    %10 = load ptr, ptr %3
    %11 = load ptr, ptr %4
    %12 = load ptr, ptr %5
    %13 = call ptr @min_element(ptr %10, ptr %11, ptr %12)
    call void @swap(ptr %9, ptr %13)
    %14 = load ptr, ptr %3
    %15 = getelementptr inbounds ptr, ptr %14, i64 1
    store ptr %15, ptr %3
    br label %L1
L3:
    ret void
}
define i1 @less(i64 %0, i64 %1) {
L0:
    %2 = alloca i64
    store i64 %0, ptr %2
    %3 = alloca i64
    store i64 %1, ptr %3
    %4 = load i64, ptr %2
    %5 = load i64, ptr %3
    %6 = icmp slt i64 %4, %5
    ret i1 %6
}
define i1 @greater(i64 %0, i64 %1) {
L0:
    %2 = alloca i64
    store i64 %0, ptr %2
    %3 = alloca i64
    store i64 %1, ptr %3
    %4 = load i64, ptr %2
    %5 = load i64, ptr %3
    %6 = icmp sgt i64 %4, %5
    ret i1 %6
}
define void @main() {
L0:
    %0 = alloca ptr
    %1 = alloca i64
    %2 = mul i64 10, 8
    %3 = call ptr @alloc(i64 %2)
    store ptr %3, ptr %0
    %4 = load ptr, ptr %0
    %5 = getelementptr inbounds i64, ptr %4, i64 0
    store i64 1, ptr %5
    %6 = load ptr, ptr %0
    %7 = getelementptr inbounds i64, ptr %6, i64 1
    store i64 2, ptr %7
    store i64 2, ptr %1
    br label %L1
L1:
    %8 = load i64, ptr %1
    %9 = icmp slt i64 %8, 10
    br i1 %9, label %L2, label %L3
L2:
    %10 = load ptr, ptr %0
    %11 = load i64, ptr %1
    %12 = sub i64 %11, 1
    %13 = getelementptr inbounds i64, ptr %10, i64 %12
    %14 = load i64, ptr %13
    %15 = load ptr, ptr %0
    %16 = load i64, ptr %1
    %17 = sub i64 %16, 2
    %18 = getelementptr inbounds i64, ptr %15, i64 %17
    %19 = load i64, ptr %18
    %20 = add i64 %14, %19
    %21 = load ptr, ptr %0
    %22 = load i64, ptr %1
    %23 = getelementptr inbounds i64, ptr %21, i64 %22
    store i64 %20, ptr %23
    %24 = load i64, ptr %1
    %25 = add i64 %24, 1
    store i64 %25, ptr %1
    br label %L1
L3:
    %26 = load ptr, ptr %0
    call void @print_int_array(ptr %26, i64 10)
    %27 = load ptr, ptr %0
    %28 = load ptr, ptr %0
    %29 = getelementptr inbounds ptr, ptr %28, i64 10
    call void @sort(ptr %27, ptr %29, ptr @greater)
    %30 = load ptr, ptr %0
    call void @print_int_array(ptr %30, i64 10)
    %31 = load ptr, ptr %0
    %32 = load ptr, ptr %0
    %33 = getelementptr inbounds ptr, ptr %32, i64 10
    call void @sort(ptr %31, ptr %33, ptr @less)
    %34 = load ptr, ptr %0
    call void @print_int_array(ptr %34, i64 10)
    %35 = load ptr, ptr %0
    call void @dealloc(ptr %35)
    ret void
}
