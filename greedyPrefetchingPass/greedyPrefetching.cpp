#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/IR/DataLayout.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Instructions.h>
#include "llvm/IR/CFG.h"
#include "llvm/IR/Value.h"

#include <queue>
#include <string>
#include <optional>
#include <iostream>
#include <vector>
#include <set>
#include <unordered_map>


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
  std::unordered_map<Value*, std::vector<std::pair<size_t, PointerType*>>> getRecursiveMemberOffsets(Function &F) {
    /***
      * For each function arg typ
      *   if type dyncasts to struct ptr
      *     For each member type of arg
      *       if member type dyncasts to struct ptr
      *         add offset to result vector
    ***/
    std::unordered_map<Value*, std::vector<std::pair<size_t, PointerType*>>> offsets;

    auto arglist = F.args();

    for (auto* a = arglist.begin(); a != arglist.end(); ++a){
      if (auto* ptr = dyn_cast<PointerType>(a->getType())) {
        if (auto* innerType = dyn_cast<StructType>(ptr->getPointerElementType())) {
          for (size_t i = 0; i < innerType->getNumElements(); ++i) {
            if (auto* fieldPtr = dyn_cast<PointerType>(innerType->getTypeAtIndex(i))) {
              if (auto* fieldInnerType = dyn_cast<StructType>(fieldPtr->getPointerElementType())) {
                offsets[a].push_back({i, fieldPtr});
              }
            }
          }
        }
      }
    }
    return offsets;
  }

  size_t countInstructions(BasicBlock& bb){
    size_t c = 0;
    for (auto& ins : bb){
      ++c;
    }
    return c;
  }

  /***
  * Generates prefetch instructions for given RDS (greedily prefetch entire RDS)
  */
  void genAndInsertPrefetchInstructions(Value* arg, std::vector<std::pair<size_t, PointerType*>>& offsets, BasicBlock* insertionBB, Function& F) {
    /***
     * Steps:
     * 1. Load the argument (this is the address of arg now)
     * For each offset:
     *   - Compute address of struct element
     *   - Prefetch addres
     * Total instructions 1 + 2 * (num_offsets)
    */
    LLVMContext& context = F.getContext();
    IRBuilder<> builder(insertionBB, insertionBB->begin());


    auto eltT = arg->getType()->getPointerElementType();

    size_t origBBSize = countInstructions(*insertionBB);
    Value* zero = ConstantInt::get(Type::getInt32Ty(context), 0);
    for (auto& [offset, prefetchPointerType] : offsets) {
        Function* prefetchFunc = Intrinsic::getDeclaration(F.getParent(), Intrinsic::prefetch, prefetchPointerType);
        // Compute address of struct element using byte offset
        Value* offsetValue = ConstantInt::get(Type::getInt32Ty(context), offset);
        Value *indices[2] = {zero, offsetValue};
        auto indexes =  ArrayRef<Value*>(indices, 2);
        errs() << " offset: " << offset << " \n";
        Value* elementAddr = builder.CreateInBoundsGEP(eltT, arg, indexes, "");
        Value* loadPtr = builder.CreateLoad(prefetchPointerType, elementAddr, "");
        if (elementAddr->getType()->isPointerTy()){
          errs() << "is pointer type: " << *elementAddr->getType() << "\n";
        }
        else{
          errs() << "is not pointer type \n";
        }
       // builder.CreateGEP(loadedArg->getType()->getPointerElementType(), loadedArg, offsetValue);

        // Prefetch address
        // 0 = read, 3 = high locality, 1 = data cache
        std::vector<Value*> args = {
            loadPtr,
            ConstantInt::get(Type::getInt32Ty(context), 0), // rw = 0 (read)
            ConstantInt::get(Type::getInt32Ty(context), 3), // locality
            ConstantInt::get(Type::getInt32Ty(context), 1)  // cache type (data cache)
        };

        builder.CreateCall(prefetchFunc->getFunctionType(), prefetchFunc, args, "");
    }
    errs() << origBBSize << " \n";
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

  BasicBlock* findFirstBlockThatNecessitatesExecutionOfOneOf(std::vector<CallInst*> targetInstructions, Function& F) {
    PostDominatorTree PDT;
    PDT.recalculate(F);

    std::vector<BasicBlock*> targetBlocks;
    for (auto* tIns : targetInstructions){
      targetBlocks.push_back(tIns->getParent());
    }
    
    std::queue<BasicBlock*> blockQueue;
    std::set<BasicBlock*> visited;

    blockQueue.push(&F.getEntryBlock());
    visited.insert(&F.getEntryBlock());

    while (!blockQueue.empty()) {
        BasicBlock *currentBlock = blockQueue.front();
        blockQueue.pop();

        for (auto* targetBlock : targetBlocks){

          if (currentBlock == targetBlock || PDT.dominates(targetBlock, currentBlock)){
            return currentBlock;
          }
        }
        

        for (BasicBlock *successor : successors(currentBlock)) {
          if (visited.find(successor) == visited.end()) {
            blockQueue.push(successor);
            visited.insert(successor);
          }
        }
    }

    return nullptr;
}

  void populateCallGraph(Module &M, CallGraph &CG) {
    for (Function& F : M.functions()) {
      llvm::CallGraphNode *cgNode = CG[&F];
      for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        if (auto *callInst = dyn_cast<CallInst>(&*I)) {
          if (auto* calledFunction = callInst->getCalledFunction()) {
            cgNode->addCalledFunction(callInst, CG[calledFunction]);
            errs() << F.getName() << " calls " << calledFunction->getName() << "\n";
            }
          }
        }
      }
    }    

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    std::unordered_map<Value*, std::vector<CallInst*>> argsToCalls = getArgumentsToCallsThatNeedIt(F);
    std::unordered_map<Value*, std::vector<std::pair<size_t, PointerType*>>> RDSTypesToOffsets = getRecursiveMemberOffsets(F);

    Module* M = F.getParent();
    llvm::CallGraph CG(*M);
    populateCallGraph(*M, CG);

    for (auto& [arg, calls] : argsToCalls) {
      if (RDSTypesToOffsets.find(arg) == RDSTypesToOffsets.end()){
        continue;
      }
      BasicBlock* insertionPoint = findFirstBlockThatNecessitatesExecutionOfOneOf(calls, F);
      if (insertionPoint) {
        errs() << "insertionPoint is: " << *insertionPoint->begin() << "\n";
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
