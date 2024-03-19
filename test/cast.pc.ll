declare ptr @alloc(i64 %0)
declare void @dealloc(ptr %0)
declare void @printint(i64 %0)
declare void @printfloat(double %0)
define i64 @main() {
L0:
    %0 = alloca double
    %1 = alloca i64
    %2 = alloca double
    %3 = alloca ptr
    %4 = alloca ptr
    %5 = alloca ptr
    store double 3.140000, ptr %0
    %6 = load double, ptr %0
    %7 = fptosi double %6 to i64
    store i64 %7, ptr %1
    %8 = load i64, ptr %1
    %9 = sitofp i64 %8 to double
    store double %9, ptr %2
    %10 = load double, ptr %0
    call void @printfloat(double %10)
    %11 = load i64, ptr %1
    call void @printint(i64 %11)
    %12 = load double, ptr %2
    call void @printfloat(double %12)
    %13 = ptrtoint ptr %1 to i64
    %14 = ptrtoint ptr %2 to i64
    %15 = sub i64 %13, %14
    call void @printint(i64 %15)
    %16 = load i64, ptr %2
    call void @printint(i64 %16)
    %17 = ptrtoint ptr %1 to i64
    %18 = inttoptr i64 %17 to ptr
    %19 = load i64, ptr %18
    call void @printint(i64 %19)
    %20 = mul i64 8, 10
    %21 = call ptr @alloc(i64 %20)
    store ptr %21, ptr %3
    %22 = load ptr, ptr %3
    %23 = getelementptr inbounds ptr, ptr %22, i64 3
    store ptr %23, ptr %4
    %24 = load ptr, ptr %3
    %25 = getelementptr inbounds ptr, ptr %24, i64 8
    store ptr %25, ptr %5
    %26 = load ptr, ptr %4
    %27 = load ptr, ptr %5
    %28 = ptrtoint ptr %26 to i64
    %29 = ptrtoint ptr %27 to i64
    %30 = sub i64 %28, %29
    %31 = sdiv i64 %30, 8
    call void @printint(i64 %31)
    %32 = load ptr, ptr %3
    call void @dealloc(ptr %32)
    ret i64 0
}
