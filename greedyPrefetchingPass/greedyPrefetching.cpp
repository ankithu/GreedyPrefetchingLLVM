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
  
  /**
   * if a recursive call derives from an argument, this will return a vector of all the instructions
   * required to calculate the pointer to the argument. Starting with the argument and 
   * ending with address passed into the call instruction.
  */
  std::optional<std::vector<Value*>> getInstructionChainFromArgToCall(CallInst* instr, Argument* arg, Value* recurseVal){
    //if the value is a store instruction we want to check 
    //if we use the address later
    if (auto* storeInst = dyn_cast<StoreInst>(recurseVal)) {
      if (storeInst->getValueOperand() == arg || storeInst->getValueOperand() == recurseVal){
        recurseVal = storeInst->getPointerOperand();
      }
    }

    //don't cross function boundaries (maybe consider how this should work properly later)
    if (auto* callInst = dyn_cast<CallInst>(recurseVal)){
      //errs() << "call inst, skipping. \n";
      return std::nullopt;
    }
    if (auto* retInst = dyn_cast<ReturnInst>(recurseVal)){
      //errs() << "ret inst, skipping. \n";
      return std::nullopt;
    }

    //errs() << "instr: " << *instr << ", val: " << *val << "\n";
    for (auto* user : recurseVal->users()){
      //errs() << "   user: " <<  *user << "\n";
      if (user == instr){
        if (auto* inst = dyn_cast<Instruction>(user)){
          for (unsigned i = 0; i < inst->getNumOperands(); ++i){
            Value *operand = inst->getOperand(i);
            if (operand == recurseVal){
              std::vector<Value*> ret = {operand};
              return ret;
            }
          }
        }
        //should never reach here
        errs() << "ruh roh \n"; 
        return std::nullopt;
      }
      auto res = getInstructionChainFromArgToCall(instr, arg, user);
      if (res){
        std::vector<Value*>& valueChain = *res;
        //I realize that this isn't really that efficient but these chains should
        //not be very long and I don't really care...
        valueChain.insert(valueChain.begin(), recurseVal);
        return valueChain;
      }
    }
    
    return std::nullopt;
  }
  

  /****
   * Returns a vector of call instructions within a given function that are recursive
  */
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

  /***
   * Returns a vector of vectors representing each possible chain of instructions
   * that leads from a function argument to a struct pointer that is passed into a
   * recursive call 
  */
  std::vector<std::vector<Value*>> getArgumentToRecursiveCallChains(Function &F){
    std::vector<std::vector<Value*>> output;

    std::vector<CallInst*> recursiveCalls = getRecursiveCalls(F);

    //the function isn't recursive so its not a candidate for this optimization.
    if (recursiveCalls.empty()){
      return output;
    }

    auto arglist = F.args();
    for (auto arg = arglist.begin(); arg != arglist.end(); ++arg){
      if (auto* pt = dyn_cast<PointerType>(arg->getType())){
        llvm::Type* inner = pt->getPointerElementType();
        //triggers if the argument is a pointer to a struct
        if (inner->isStructTy()){
          llvm::StructType* st = (llvm::StructType*) inner;
          //for every struct pointer argument we check every recursive call in the function to see
          //if there is a usage chain 
          for (auto* recursiveCall : recursiveCalls){
            auto res = getInstructionChainFromArgToCall(recursiveCall, arg, arg);
            if (res){
              output.push_back(*res);
              errs() << "Prefetch candidate identified! \n original arugment: " << *arg << " \n";
              for (auto* val : *res){
                errs() << "value: " << *val << "\n";
              }
              errs() << "------ \n";
            }
          }
        }
      }
    }
    return output;
  }

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    std::vector<std::vector<Value*>> prefetchChains = getArgumentToRecursiveCallChains(F);
    
    
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

