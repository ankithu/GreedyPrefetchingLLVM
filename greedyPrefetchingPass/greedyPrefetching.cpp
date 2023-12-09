#include "llvm/IR/Dominators.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/IR/DataLayout.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Instructions.h>

#include <string>
#include <optional>
#include <iostream>
#include <vector>
#include <utility>

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
      if (isChainFromArgToCall(instr, arg, user)){
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
 // TODO: check that structs are not opaque before peering into them
  std::unordered_map<Value*, std::vector<std::pair<Type*, size_t>>> getRecursiveMemberOffsets(Function &F) {
    /***
      * For each function arg typ
      *   if type dyncasts to struct ptr
      *     For each member type of arg
      *       if member type dyncasts to struct ptr
      *         add offset to result vector
    ***/
    Module* module = F.getParent();
    const DataLayout& DL = module->getDataLayout();
    std::unordered_map<Value*, std::vector<std::pair<Type*, size_t>>> offsets;

    auto arglist = F.args();

    for (auto* a = arglist.begin(); a != arglist.end(); ++a){
      if (auto* ptr = dyn_cast<PointerType>(a->getType())) {
        if (auto* innerType = dyn_cast<StructType>(ptr->getElementType())) {
          for (size_t i = 0; i < innerType->getNumElements(); ++i) {
            auto* fieldType = innerType->getTypeAtIndex(i);

            if (auto* fieldPtr = dyn_cast<PointerType>(fieldType)) {
              if (auto* fieldInnerType = dyn_cast<StructType>(fieldPtr->getElementType())) {
                uint64_t offset = DL.getStructLayout(innerType)->getElementOffset(i);
                offsets[a].push_back({fieldType, offset});

                std::string type_str;
                llvm::raw_string_ostream rso(type_str);
                fieldInnerType->print(rso);
                llvm::errs() << "Type Info: " << rso.str() << " " << offset << " " << "\n";
              }
            }

          }
        }
      }
    }
    return offsets;
  }

  /***
  * Generates prefetch instructions for given RDS (greedily prefetch entire RDS)
  */
  void genAndInsertPrefetchInstructions(Value* arg, std::vector<size_t>& offsets, BasicBlock* insertionBB, Function& F) {
    /***
     * Steps:
     * 1. Load the argument (this is the address of arg now)
     * For each offset:
     *   - Compute address of struct element
     *   - Prefetch addres
     * Total instructions 1 + 2 * (num_offsets)
    */
    LLVMContext& context = F.getContext();

    std::vector<Instruction*> prefetchInstructions;


    // Load the value of arg (the address of the struct)    
    auto eltT = arg->getType()->getPointerElementType();
    errs() << *eltT << " a:" << *arg << "\n";
    LoadInst* loadedArg = new LoadInst(eltT, arg, "", &*insertionBB->begin());
    prefetchInstructions.push_back(loadedArg);
    errs() << F << " \n \n \n" << *F.getParent() << " \n \n";
    Function* prefetchFunc = Intrinsic::getDeclaration(F.getParent(), Intrinsic::prefetch);

    for (size_t offset : offsets) {
        // Compute address of struct element using byte offset
        Value* offsetValue = ConstantInt::get(Type::getInt64Ty(context), offset);
        // Create a new GEP instruction (dangling, not yet attached to a basic block)
        Value* elementAddr = GetElementPtrInst::Create(
                                  eltT, // The element type
                                  loadedArg, // The base pointer
                                  {offsetValue}, // The index list
                                  "",
                                  &*insertionBB->begin()
                            );

       // builder.CreateGEP(loadedArg->getType()->getPointerElementType(), loadedArg, offsetValue);

        // Prefetch address
        // 0 = read, 3 = high locality, 1 = data cache
        std::vector<Value*> args = {
            elementAddr,
            ConstantInt::get(Type::getInt32Ty(context), 0), // rw = 0 (read)
            ConstantInt::get(Type::getInt32Ty(context), 3), // locality
            ConstantInt::get(Type::getInt32Ty(context), 1)  // cache type (data cache)
        };

        // Create a new CallInst (dangling, not yet attached to a basic block)
        CallInst* prefetchInst = CallInst::Create(prefetchFunc->getFunctionType(), prefetchFunc, args, "", &*insertionBB->begin());

    }

    
  
  }
  
  /***
   * Returns map from call to vector of Arguments that it relies on
  */
  std::unordered_map<Value*, std::vector<CallInst*>> getArgumentsToCallsThatNeedIt(Function &F){
    std::unordered_map<Value*, std::vector<CallInst*>> output;
    auto arglist = F.args();
    std::vector<CallInst*> recursiveCalls = getRecursiveCalls(F);

    //the function isn't recursive so its not a candidate for this optimization.
    if (recursiveCalls.empty()){
      return output;
    }

    for (auto arg = arglist.begin(); arg != arglist.end(); ++arg){
      for (auto* recursiveCall : recursiveCalls){
        if (isChainFromArgToCall(recursiveCall, arg, arg)) {
          output[arg].push_back(recursiveCall);
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



  std::unordered_map<BasicBlock*, size_t> getStaticOrdering(Function &F){
    //gets a static ordering of the basic blocks to give an approximate sequence
    size_t i = 0;
    std::unordered_map<BasicBlock*, size_t> output;
    for (auto& bb : F){
      output[&bb] = i;
      ++i;
    }
    return output;
  }

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    std::unordered_map<Value*, std::vector<CallInst*>> argsToCalls = getArgumentsToCallsThatNeedIt(F);
    std::unordered_map<Value*, std::vector<std::pair<Type*, size_t>>> RDSTypesToOffsets = getRecursiveMemberOffsets(F);
    std::unordered_map<BasicBlock*, size_t> order = getStaticOrdering(F);

    for (auto& [arg, calls] : argsToCalls) {
      if (RDSTypesToOffsets.find(arg) == RDSTypesToOffsets.end()){
        continue;
      }
      BasicBlock* insertionPoint = nullptr;
      for (auto* call : calls) {
        BasicBlock* candidateInsertionPoint = getIDom(F, call);
        if (!insertionPoint || order[candidateInsertionPoint] < order[insertionPoint]){
          insertionPoint = candidateInsertionPoint;
        }
      }
      if (insertionPoint) {
        genAndInsertPrefetchInstructions(arg, RDSTypesToOffsets[arg], insertionPoint, F);
      }
      else{
        errs() << "no insertion point calculated. This is probably wrong! \n";
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
