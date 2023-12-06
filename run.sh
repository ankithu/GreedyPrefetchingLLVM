clang -emit-llvm -S $1.c -Xclang -disable-O0-optnone -o $1.ll

opt -disable-output -load-pass-plugin=./build/greedyPrefetchingPass/GreedyPrefetch.so -passes="greedy-prefetch" $1.ll