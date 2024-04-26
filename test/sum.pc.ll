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
define ptr @plus(ptr %0, ptr %1) {
L0:
    %2 = alloca ptr
    store ptr %0, ptr %2
    %3 = alloca ptr
    store ptr %1, ptr %3
    %4 = load ptr, ptr %2
    %5 = ptrtoint ptr %4 to i64
    %6 = load ptr, ptr %3
    %7 = ptrtoint ptr %6 to i64
    %8 = add i64 %5, %7
    %9 = inttoptr i64 %8 to ptr
    ret ptr %9
}
define ptr @deref(ptr %0) {
L0:
    %1 = alloca ptr
    store ptr %0, ptr %1
    %2 = load ptr, ptr %1
    %3 = load i64, ptr %2
    %4 = inttoptr i64 %3 to ptr
    ret ptr %4
}
define ptr @range_sum(ptr %0, i64 %1, i64 %2) {
L0:
    %3 = alloca ptr
    store ptr %0, ptr %3
    %4 = alloca i64
    store i64 %1, ptr %4
    %5 = alloca i64
    store i64 %2, ptr %5
    %6 = alloca i64
    %7 = alloca ptr
    %8 = alloca i64
    store i64 0, ptr %6
    %9 = load ptr, ptr %3
    store ptr %9, ptr %7
    store i64 0, ptr %8
    br label %L1
L1:
    %10 = load i64, ptr %8
    %11 = load i64, ptr %5
    %12 = icmp slt i64 %10, %11
    br i1 %12, label %L2, label %L3
L2:
    %13 = load i64, ptr %6
    %14 = load ptr, ptr %7
    %15 = load i64, ptr %4
    %16 = load i64, ptr %8
    %17 = add i64 %15, %16
    %18 = getelementptr inbounds i64, ptr %14, i64 %17
    %19 = load i64, ptr %18
    %20 = add i64 %13, %19
    store i64 %20, ptr %6
    %21 = load i64, ptr %8
    %22 = add i64 %21, 1
    store i64 %22, ptr %8
    br label %L1
L3:
    %23 = load i64, ptr %6
    %24 = inttoptr i64 %23 to ptr
    ret ptr %24
}
define void @main() {
L0:
    %0 = alloca i64
    %1 = alloca ptr
    %2 = alloca i64
    %3 = alloca double
    %4 = alloca i64
    %5 = alloca double
    %6 = alloca i64
    %7 = alloca double
    %8 = alloca ptr
    %9 = alloca i64
    store i64 100000000, ptr %0
    %10 = load i64, ptr %0
    %11 = mul i64 8, %10
    %12 = call ptr @alloc(i64 %11)
    store ptr %12, ptr %1
    store i64 0, ptr %2
    br label %L1
L1:
    %13 = load i64, ptr %2
    %14 = load i64, ptr %0
    %15 = icmp slt i64 %13, %14
    br i1 %15, label %L2, label %L3
L2:
    %16 = load i64, ptr %2
    %17 = add i64 %16, 1
    %18 = load ptr, ptr %1
    %19 = load i64, ptr %2
    %20 = getelementptr inbounds i64, ptr %18, i64 %19
    store i64 %17, ptr %20
    %21 = load i64, ptr %2
    %22 = add i64 %21, 1
    store i64 %22, ptr %2
    br label %L1
L3:
    store i64 0, ptr %2
    %23 = call double @pc_time()
    %24 = fneg double %23
    store double %24, ptr %3
    store i64 0, ptr %4
    br label %L4
L4:
    %25 = load i64, ptr %2
    %26 = load i64, ptr %0
    %27 = icmp slt i64 %25, %26
    br i1 %27, label %L5, label %L6
L5:
    %28 = load i64, ptr %4
    %29 = load ptr, ptr %1
    %30 = load i64, ptr %2
    %31 = getelementptr inbounds i64, ptr %29, i64 %30
    %32 = load i64, ptr %31
    %33 = add i64 %28, %32
    store i64 %33, ptr %4
    %34 = load i64, ptr %2
    %35 = add i64 %34, 1
    store i64 %35, ptr %2
    br label %L4
L6:
    %36 = load double, ptr %3
    %37 = call double @pc_time()
    %38 = fadd double %36, %37
    store double %38, ptr %3
    %39 = load i64, ptr %4
    call void @printint(i64 %39)
    %40 = load double, ptr %3
    %41 = fmul double %40, 1000.000000
    call void @printfloat(double %41)
    %42 = call double @pc_time()
    %43 = fneg double %42
    store double %43, ptr %5
    %44 = load ptr, ptr %1
    %45 = load i64, ptr %0
    %46 = call ptr @parallel_reduce(ptr %44, i64 8, i64 %45, ptr @deref, ptr @plus)
    %47 = ptrtoint ptr %46 to i64
    store i64 %47, ptr %6
    %48 = load double, ptr %5
    %49 = call double @pc_time()
    %50 = fadd double %48, %49
    store double %50, ptr %5
    %51 = load i64, ptr %6
    call void @printint(i64 %51)
    %52 = load double, ptr %5
    %53 = fmul double %52, 1000.000000
    call void @printfloat(double %53)
    %54 = call double @pc_time()
    %55 = fneg double %54
    store double %55, ptr %7
    %56 = load ptr, ptr %1
    %57 = load i64, ptr %0
    %58 = call ptr @parallel_for(ptr %56, i64 %57, ptr @range_sum)
    store ptr %58, ptr %8
    store i64 0, ptr %9
    store i64 0, ptr %2
    br label %L7
L7:
    %59 = load i64, ptr %2
    %60 = icmp slt i64 %59, 16
    br i1 %60, label %L8, label %L9
L8:
    %61 = load i64, ptr %9
    %62 = load ptr, ptr %8
    %63 = load i64, ptr %2
    %64 = getelementptr inbounds i64, ptr %62, i64 %63
    %65 = load i64, ptr %64
    %66 = add i64 %61, %65
    store i64 %66, ptr %9
    %67 = load i64, ptr %2
    %68 = add i64 %67, 1
    store i64 %68, ptr %2
    br label %L7
L9:
    %69 = load double, ptr %7
    %70 = call double @pc_time()
    %71 = fadd double %69, %70
    store double %71, ptr %7
    %72 = load i64, ptr %9
    call void @printint(i64 %72)
    %73 = load double, ptr %7
    %74 = fmul double %73, 1000.000000
    call void @printfloat(double %74)
    ret void
}
