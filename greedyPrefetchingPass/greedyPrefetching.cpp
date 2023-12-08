#include "llvm/IR/Dominators.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/IR/DataLayout.h"

#include <optional>
#include  <iostream>

using namespace llvm;


namespace {

struct GreedyPrefetchPass : public PassInfoMixin<GreedyPrefetchPass> {
  
  /**
   * if a recursive call derives from an argument, this will return true
  */
  bool isChainFromArgToCall(CallInst* instr, Argument* arg, Value* recurseVal){
    //if the value is a store instruction we want to check 
    //if we use the address later
    if (auto* storeInst = dyn_cast<StoreInst>(recurseVal)) {
      if (storeInst->getValueOperand() == arg || storeInst->getValueOperand() == recurseVal) {
        recurseVal = storeInst->getPointerOperand();
      }
    }

    //don't cross function boundaries (maybe consider how this should work properly later)
    if (auto* callInst = dyn_cast<CallInst>(recurseVal)) {
      //errs() << "call inst, skipping. \n";
      return false;
    }
    if (auto* retInst = dyn_cast<ReturnInst>(recurseVal)) {
      //errs() << "ret inst, skipping. \n";
      return false;
    }

    //errs() << "instr: " << *instr << ", val: " << *val << "\n";
    for (auto* user : recurseVal->users()){
      //errs() << "   user: " <<  *user << "\n";
      if (user == instr){
        if (auto* inst = dyn_cast<Instruction>(user)){
          for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
            Value *operand = inst->getOperand(i);
            if (operand == recurseVal){
              return true;
            }
          }
        }
        //should never reach here
        errs() << "ruh roh \n"; 
        return false;
      }
      if (getInstructionChainFromArgToCall(instr, arg, user)){
        return true;
      }
    }
    return false;
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
    * Returns a map from argument of function args to offsets of record pointer members
  ***/
  std::unordered_map<Value*, std::vector<size_t>> getRecursiveMemberOffsets(Function &F) {
    /***
      * For each function arg typ
      *   if type dyncasts to struct ptr
      *     For each member type of arg
      *       if member type dyncasts to struct ptr
      *         add offset to result vector
    ***/
    for (auto* a : F.args()) {
      if (auto* ptr = dyn_cast<PointerType>(a->type) && auto*) {
        // FIXME: 
  e
      }
    }
  }

  /***
  * Generates prefetch instructions for given RDS (greedily prefetch entire RDS)
  */
  std::vector<Instruction*> getPrefetchInstructions(Value* arg, std::vector<size_t>& offsets) {

  }
  
  /***
   * Returns map from call to vector of Arguments that it relies on
  */
  std::unordered_map<CallInst*, std::vector<Value*>> getCallsToRequiredArguments(Function &F){
    std::unordered_map<CallInst*, std::vector<Value*>> output;
    auto arglist = F.args();
    std::vector<CallInst*> recursiveCalls = getRecursiveCalls(F);

    //the function isn't recursive so its not a candidate for this optimization.
    if (recursiveCalls.empty()){
      return output;
    }

    for (auto arg = arglist.begin(); arg != arglist.end(); ++arg){
      for (auto* recursiveCall : recursiveCalls){
        if (getInstructionChainFromArgToCall(recursiveCall, arg, arg)) {
          output[recursiveCall].push_back(arg);
        }
      }
    }
    return output;
  }

  BasicBlock* getIDom(Function& F, Value* target){
    // brute force iteration over all instructions to find idom of what we want to prefetch
    llvm::DominatorTree DT;
    
    DT.recalculate(F);
    if (auto* targetInstr = dyn_cast<Instruction>(target)){
      for (auto& bb : F) {
        for (auto& instr: bb) {
          if (DT.dominates(&instr, targetInstr)) {
            return &bb;
          }
        }
      }
    }
    return nullptr;
  }

  void insertPrefetchesAtStartOfBB(BasicBlock* bb, std::vector<Value*> prefetches){
    for (auto chain_itr = prefetches.begin(); chain_itr !=  prefetches.end(); ++chain_itr) {
      if (auto* instToCopy = dyn_cast<Instruction>(*chain_itr)) {
        Instruction* newInst = instToCopy->clone();
        bb->getInstList().insert(bb->begin(), newInst);
      }
    }
  }

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    std::unordered_map<CallInst*, std::vector<Value*>> callsToRequiredArgs = getCallsToRequiredArguments(F);
    std::unordered_map<Value*, std::vector<size_t>> RDSTypesToOffsets = getRecursiveMemberOffsets(F);
    

    for (auto& [callInst, requiredArgs] : callsToRequiredArgs) {
      for (auto* arg : requiredArgs) {
        if (RDSTypesToOffsets.find(arg) != RDSTypesToOffsets.end()) {
          BasicBlock* bbToInsertPrefetches = getIDom(F, callInst);
          std::vector<Instruction*> prefetches = getPrefetchInstructions(arg, RDSTypesToOffsets[arg]);
          insertPrefetchesAtStartOfBB(bbToInsertPrefetches, prefetches);
        }
      }
    }

    for (auto& bb : F){
      for (auto& i : bb){
        errs() << i << "\n";
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
          if (Name == "greedy-prefetch") {
            FPM.addPass(GreedyPrefetchPass());
            return true;
          }
          return false;
        }
      );
    }
  };
}
