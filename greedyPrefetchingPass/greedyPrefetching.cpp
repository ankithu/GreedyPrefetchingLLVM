#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include  <iostream>

using namespace llvm;

namespace {

struct FunctionNamePass : public PassInfoMixin<FunctionNamePass> {

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {

    for (auto bb = F.begin(); bb != F.end(); ++bb){
      std::cout << "START basic block with name: " << bb->getName().str() << " in function " << F.getName().str() << std::endl;
      for (auto op = bb->begin(); op != bb->end(); ++op){
        std::cout << "op: " << op->getOpcodeName() << std::endl;
      }
    }
    return PreservedAnalyses::all();
  }
};
}

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "GreedyPrefetch", "v0.1",
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef Name, FunctionPassManager &FPM,
        ArrayRef<PassBuilder::PipelineElement>) {
          if(Name == "greedy-prefetch"){
            FPM.addPass(FunctionNamePass());
            return true;
          }
          return false;
        }
      );
    }
  };
}

