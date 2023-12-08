; ModuleID = 'tests/test.c'
source_filename = "tests/test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

%struct.TreeNode = type { i32, %struct.TreeNode*, %struct.TreeNode* }

@.str = private unnamed_addr constant [3 x i8] c"%d\00", align 1

; Function Attrs: noinline nounwind uwtable
define dso_local void @sumPreorder(%struct.TreeNode* noundef %0) #0 {
  %2 = alloca %struct.TreeNode*, align 8
  store %struct.TreeNode* %0, %struct.TreeNode** %2, align 8
  %3 = load %struct.TreeNode*, %struct.TreeNode** %2, align 8
  %4 = icmp ne %struct.TreeNode* %3, null
  br i1 %4, label %6, label %5

5:                                                ; preds = %1
  br label %17

6:                                                ; preds = %1
  %7 = load %struct.TreeNode*, %struct.TreeNode** %2, align 8
  %8 = getelementptr inbounds %struct.TreeNode, %struct.TreeNode* %7, i32 0, i32 0
  %9 = load i32, i32* %8, align 8
  %10 = call i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([3 x i8], [3 x i8]* @.str, i64 0, i64 0), i32 noundef %9)
  %11 = load %struct.TreeNode*, %struct.TreeNode** %2, align 8
  %12 = getelementptr inbounds %struct.TreeNode, %struct.TreeNode* %11, i32 0, i32 1
  %13 = load %struct.TreeNode*, %struct.TreeNode** %12, align 8
  call void @sumPreorder(%struct.TreeNode* noundef %13)
  %14 = load %struct.TreeNode*, %struct.TreeNode** %2, align 8
  %15 = getelementptr inbounds %struct.TreeNode, %struct.TreeNode* %14, i32 0, i32 2
  %16 = load %struct.TreeNode*, %struct.TreeNode** %15, align 8
  call void @sumPreorder(%struct.TreeNode* noundef %16)
  br label %17

17:                                               ; preds = %6, %5
  ret void
}

declare i32 @printf(i8* noundef, ...) #1

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca %struct.TreeNode, align 8
  store i32 0, i32* %1, align 4
  call void @sumPreorder(%struct.TreeNode* noundef %2)
  ret i32 0
}

attributes #0 = { noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 1}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Ubuntu clang version 14.0.0-1ubuntu1.1"}
