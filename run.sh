clang -emit-llvm -S tests/$1.c -Xclang -disable-O0-optnone -o $1.ll

opt -load-pass-plugin=./build/greedyPrefetchingPass/GreedyPrefetch.so -S -passes="greedy-prefetch" $1.ll -o ${1}.prof.bc