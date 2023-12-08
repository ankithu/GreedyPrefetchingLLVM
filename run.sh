#!/bin/bash
# clang -emit-llvm -S tests/$1.c -Xclang -disable-O0-optnone -o $1.ll

# opt -load-pass-plugin=./build/greedyPrefetchingPass/GreedyPrefetch.so -S -passes="greedy-prefetch" $1.ll -o ${1}.prof.bc
# TODO: Potentially add Profiler Data/Information as necessary

PATH2LIB="./build/greedyPrefetchingPass/GreedyPrefetch.so"

PASS=greedy-prefetch

# Clean out any last profiler/pass/bytecode/output/ll files
rm -f default.profraw *_prof *_greedy *.bc *.profdata *_output *.ll

## Convert to bytecode
clang -emit-llvm -c tests/$1.c -Xclang -disable-O0-optnone -o $1.bc

## Output the regular executable, no passes done
clang ${1}.bc -o ${1}.exe

# When we run the profiler embedded executable, it generates a default.profraw file that contains the profile data.
# ./${1}.exe > correct_output TODO: UPDATE EXAMPLES FOR OUTPUT REASONS

opt -load-pass-plugin="./build/greedyPrefetchingPass/GreedyPrefetch.so" -passes="greedy-prefetch" ${1}.bc -o ${1}.greedy.bc

clang ${1}.greedy.bc -o ${1}_greedy.exe

# ./${1}_greedy > greedy_output TODO: UPDATE EXAMPLES FOR OUTPUT REASONS


# echo -e "\n=== Program Correctness Validation ==="
# if [ "$(diff correct_output greedy_output)" != "" ]; then
#     echo -e ">> Outputs do not match\n"
# else
#     echo -e ">> Outputs match\n"
#     # Measure performance
#     echo -e "1. Performance of unoptimized code"
#     time ./${1} > /dev/null
#     echo -e "\n\n"
#     echo -e "2. Performance of optimized code"
#     time ./${1}_greedy > /dev/null
#     echo -e "\n\n"
# fi

# Old example
# opt -load-pass-plugin=./build/greedyPrefetchingPass/GreedyPrefetch.so -S -passes="greedy-prefetch" $1.ll -o ${1}.prof.bc