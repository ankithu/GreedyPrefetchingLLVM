#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/DependenceAnalysis.h"

#include  <iostream>

using namespace llvm;

namespace {

struct GreedyPrefetchPass : public PassInfoMixin<GreedyPrefetchPass> {

  bool isDependent(Value *operand, Function *function) {
    //buggy right now...
    if (isa<Argument>(operand)) {
        return true;
    }

    if (auto *inst = dyn_cast<Instruction>(operand)) {
        if (isa<AllocaInst>(inst)) {
            // For alloca instructions, check the uses of the alloca
            for (auto *user : inst->users()) {
                if (auto *userInst = dyn_cast<Instruction>(user)) {
                    for (unsigned i = 0; i < userInst->getNumOperands(); ++i) {
                        if (isDependent(userInst->getOperand(i), function)) {
                            return true;
                        }
                    }
                }
            }
        } else {
            // For other instructions, recursively check each operand
            for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
                if (isDependent(inst->getOperand(i), function)) {
                    return true;
                }
            }
        }
    }

    return false;
}

  std::vector<CallInst*> getRecursiveCalls(Function &F) {
    std::vector<CallInst*> res;
    for (auto bb = F.begin(); bb != F.end(); ++bb){
      for (auto op  = bb->begin(); op != bb->end(); ++op){
        if (CallInst *callInst = dyn_cast<CallInst>(op)) {
          if (Function *calledFunction = callInst->getCalledFunction()) {
            if (calledFunction == &F){
              res.push_back(callInst);
            }
          }
        }
      }
    }
    return res;
  }

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {

    auto arglist = F.args();
    std::vector<CallInst*> recursiveCalls = getRecursiveCalls(F);
    if (recursiveCalls.empty()){
      return PreservedAnalyses::all();
    }

    std::cout << "function " << std::endl;
    
    for (auto a = arglist.begin(); a != arglist.end(); ++a){
      if (PointerType* pt = dyn_cast<PointerType>(a->getType())){
        llvm::Type* inner = pt->getPointerElementType();
        if (inner->isStructTy()){
          llvm::StructType* st = (llvm::StructType*) inner;
          std::cout << "struct pointer: " << st->getName().str() << std::endl;
          for (auto* recursiveCall : recursiveCalls){
            recursiveCall->print(errs());
            errs() <<"\n"<< "\n";
            for (unsigned i = 0; i < recursiveCall->getNumOperands(); ++i) {
                if (isDependent(recursiveCall->getOperand(i), &F)) {
                    errs() << "Call instruction in function " << F.getName() << " is dependent on its input argument.\n";
                    recursiveCall->print(errs());
                    errs() << "\n";
                    break;
                }
              }
          }
        }
        //std::cout << "pointer: " << std::endl;
      }
    }
    

    // for (auto bb = F.begin(); bb != F.end(); ++bb){
    //   std::cout << "START basic block with name: " << bb->getName().str() << " in function " << F.getName().str() << std::endl;
    //   for (auto op = bb->begin(); op != bb->end(); ++op){
    //     std::cout << "op: " << op->getOpcodeName() << std::endl;
    //   }
    // }
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
            FPM.addPass(GreedyPrefetchPass());
            return true;
          }
          return false;
        }
      );
    }
  };
}

