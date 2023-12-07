#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include <optional>
#include  <iostream>

using namespace llvm;


namespace {

struct GreedyPrefetchPass : public PassInfoMixin<GreedyPrefetchPass> {
  
  //determines if a given instruction uses a given pointer value
  std::optional<Value*> usesValue(Value* instr, Value* val, Value* orig){
    //if the value is a store instruction we want to check 
    //if we use the address later
    if (auto* storeInst = dyn_cast<StoreInst>(val)) {
      if (storeInst->getValueOperand() == orig || storeInst->getValueOperand() == val){
        val = storeInst->getPointerOperand();
      }
    }

    //don't cross function boundaries (maybe consider how this should work properly later)
    if (auto* callInst = dyn_cast<CallInst>(val)){
      //errs() << "call inst, skipping. \n";
      return std::nullopt;
    }
    if (auto* retInst = dyn_cast<ReturnInst>(val)){
      //errs() << "ret inst, skipping. \n";
      return std::nullopt;
    }

    //errs() << "instr: " << *instr << ", val: " << *val << "\n";

    for (auto* user : val->users()){
      //errs() << "   user: " <<  *user << "\n";
      if (user == instr){
        if (auto* inst = dyn_cast<Instruction>(user)){
          for (unsigned i = 0; i < inst->getNumOperands(); ++i){
            Value *operand = inst->getOperand(i);
            if (operand == val){
              errs() << " specific: " << *operand << "\n";
              return operand;
            }
          }
        }
        //should never reach here
        errs() << "ruh roh \n"; 
        return std::nullopt;
      }
      auto res = usesValue(instr, user, orig);
      if (res){
        return res;
      }
    }
    
    return std::nullopt;
  }
  
  //returns a vector of call isntructions in a function that are calls to the
  //same function
  std::vector<CallInst*> getRecursiveCalls(Function& F) {
    std::vector<CallInst*> res;
    for (auto& bb: F){
      for (auto& instr: bb){
        if (auto* callInst = dyn_cast<CallInst>(&instr)) {
          if (auto* calledFunction = callInst->getCalledFunction()) {
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

    std::vector<CallInst*> recursiveCalls = getRecursiveCalls(F);
    //the function isn't recursive so our optimization doesn't work.
    if (recursiveCalls.empty()){
      return PreservedAnalyses::all();
    }

    auto arglist = F.args();
    for (auto arg = arglist.begin(); arg != arglist.end(); ++arg){
      if (auto* pt = dyn_cast<PointerType>(arg->getType())){
        llvm::Type* inner = pt->getPointerElementType();
        //triggers if the argument is a pointer to a struct
        if (inner->isStructTy()){
          llvm::StructType* st = (llvm::StructType*) inner;
          
          for (auto* recursiveCall : recursiveCalls){
            auto res = usesValue(recursiveCall, arg, arg);
            if (res){
              
              errs() << "Prefetch candidate, arg: " << *arg
              << ", load to prefetch: " << **res << "\n";
            }
            
          }
          
        }
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
            FPM.addPass(GreedyPrefetchPass());
            return true;
          }
          return false;
        }
      );
    }
  };
}

