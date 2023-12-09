#include "llvm/Analysis/PostDominators.h"
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

#include <queue>
#include <string>
#include <optional>
#include <iostream>
#include <vector>
#include <set>

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
  std::unordered_map<Value*, std::vector<size_t>> getRecursiveMemberOffsets(Function &F) {
    /***
      * For each function arg typ
      *   if type dyncasts to struct ptr
      *     For each member type of arg
      *       if member type dyncasts to struct ptr
      *         add offset to result vector
    ***/
    Module* module = F.getParent();
    const DataLayout& DL = module->getDataLayout();
    std::unordered_map<Value*, std::vector<size_t>> offsets;

    auto arglist = F.args();

    for (auto* a = arglist.begin(); a != arglist.end(); ++a){
      if (auto* ptr = dyn_cast<PointerType>(a->getType())) {
        if (auto* innerType = dyn_cast<StructType>(ptr->getElementType())) {
          for (size_t i = 0; i < innerType->getNumElements(); ++i) {
            auto* fieldType = innerType->getTypeAtIndex(i);

            if (auto* fieldPtr = dyn_cast<PointerType>(fieldType)) {
              if (auto* fieldInnerType = dyn_cast<StructType>(fieldPtr->getElementType())) {
                uint64_t offset = DL.getStructLayout(innerType)->getElementOffset(i);
                offsets[a].push_back(offset);

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
    IRBuilder<> builder(insertionBB, insertionBB->begin());


    auto eltT = arg->getType()->getPointerElementType();
    Function* prefetchFunc = Intrinsic::getDeclaration(F.getParent(), Intrinsic::prefetch, eltT);

    size_t origBBSize = countInstructions(*insertionBB);
    for (size_t offset : offsets) {
        // Compute address of struct element using byte offset
        Value* offsetValue = ConstantInt::get(Type::getInt32Ty(context), offset);
        // Create a new GEP instruction (dangling, not yet attached to a basic block)
        Instruction* elementAddr = GetElementPtrInst::Create(
                                  eltT, // The element type
                                  arg, // The base pointer
                                  {offsetValue}, // The index list
                                  "",
                                  insertionBB->getTerminator()
                            );
        //Value* elementAddr = builder.CreateGEP(eltT, arg, {offsetValue}, "");
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
        CallInst* prefetchInst = CallInst::Create(prefetchFunc->getFunctionType(), prefetchFunc, args, "");
        prefetchInst->insertBefore(insertionBB->getTerminator());
        //Value* call = builder.CreateCall(prefetchFunc->getFunctionType(), prefetchFunc, args, "");
    }
    errs() << origBBSize << " \n";
    // for (size_t i = 0; i < offsets.size(); ++i){
    //   // Get the terminator instruction
    //   Instruction *terminator = insertionBB->getTerminator();

    //   // Get the instruction before the terminator
    //   Instruction *instrToMove = terminator->getPrevNode();

    //   // Remove from its current position
    //   instrToMove->removeFromParent();

    //   // Insert at the beginning of the basic block
    //   insertionBB->getInstList().insert(insertionBB->getFirstInsertionPt(), instrToMove);
    // }
    // for (size_t i = 0; i < origBBSize; ++i){
    //   Instruction* firstInst = &*insertionBB->begin();
    //   Instruction* terminatorInst = insertionBB->getTerminator();
    //   errs() << "f:" << firstInst << " \n" << "l: " << terminatorInst << " \n";
    //   if (firstInst != terminatorInst) {
    //     firstInst->moveBefore(terminatorInst);
    //   }
    // }
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


  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    std::unordered_map<Value*, std::vector<CallInst*>> argsToCalls = getArgumentsToCallsThatNeedIt(F);
    std::unordered_map<Value*, std::vector<size_t>> RDSTypesToOffsets = getRecursiveMemberOffsets(F);

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
