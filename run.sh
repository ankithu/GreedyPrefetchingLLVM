#!/bin/bash
PATH2LIB="./build/greedyPrefetchingPass/GreedyPrefetch.so"
PASS=greedy-prefetch

# Clean out any last profiler/pass/bytecode/output/ll files
rm -f default.profraw *_prof *_greedy *.bc *.profdata *_output *.ll *.exe *.s

## Convert to bytecode
clang -emit-llvm -c tests/${1}.c -Xclang -disable-O0-optnone -o ${1}.bc

## Run pass for instrument profiler
opt -passes='pgo-instr-gen,instrprof' ${1}.bc -o ${1}.prof.bc

# Note: We are using the New Pass Manager for these passes! 
# Generate binary executable with profiler embedded
clang -fprofile-instr-generate ${1}.prof.bc -o ${1}_prof.exe

# Print Correct Output (note that this is just the normal one with profiler data added to it)
./${1}_prof.exe > correct_output

llvm-profdata merge -o ${1}.profdata default.profraw

opt -passes="pgo-instr-use" -o ${1}.profdata.bc -pgo-test-profile-file=${1}.profdata < ${1}.prof.bc > /dev/null

## OLD CODE : Output the regular executable, no passes done
# clang ${1}.bc -o ${1}.exe

# When we run the profiler embedded executable, it generates a default.profraw file that contains the profile data.
# ./${1}.exe > correct_output TODO: UPDATE EXAMPLES FOR OUTPUT REASONS

opt -load-pass-plugin="./build/greedyPrefetchingPass/GreedyPrefetch.so" -passes="greedy-prefetch" ${1}.profdata.bc -o ${1}_greedy.bc
clang ${1}_greedy.bc -o ${1}_greedy.exe

# get ll files for debugging
llvm-dis ${1}.bc -o ${1}.ll # without profiling
llvm-dis ${1}_greedy.bc -o ${1}_greedy.ll
llc -march=x86 -o ${1}_greedy.s ${1}_greedy.ll

./${1}_greedy.exe > greedy_output

echo -e "\n=== Program Correctness Validation ==="
if [ "$(diff correct_output greedy_output)" != "" ]; then
    echo -e ">> Outputs do not match\n"
 else
    echo -e ">> Outputs match\n"
fi

rm greedy_output
rm correct_output

# Measure performance
echo -e "1. Performance of unoptimized code"
time ./${1}_prof.exe > /dev/null
echo -e "\n\n"
echo -e "2. Performance of optimized code"
time ./${1}_greedy.exe > /dev/null
echo -e "\n\n"

# Old example (before adding the passes and profiler information)
# clang -emit-llvm -S tests/$1.c -Xclang -disable-O0-optnone -o $1.ll
# opt -load-pass-plugin=./build/greedyPrefetchingPass/GreedyPrefetch.so -S -passes="greedy-prefetch" $1.ll -o ${1}.prof.bc