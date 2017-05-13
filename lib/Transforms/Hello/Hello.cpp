//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Comdat.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalAlias.h"
#include "llvm/IR/GlobalIFunc.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Constants.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Hello.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

#define DEBUG_TYPE "hello"

STATISTIC(HelloCounter, "Counts number of functions greeted");

namespace {
  // Hello - The first implementation, without getAnalysisUsage.
  struct Hello : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    Hello() : FunctionPass(ID) {
		initializeHelloPass(*PassRegistry::getPassRegistry());
	}

    bool runOnFunction(Function &F) override 
	{
		++HelloCounter;
		for (auto& basicBlock : F)
		{
			for (auto& inst : basicBlock)
			{
				errs() << "Inst: ";
				errs().write_escaped(inst.getName()) << '\n';
			}
		}
		return false;
    }
  };
}

char Hello::ID = 0;
INITIALIZE_PASS(Hello, "hello", "Hello world pass", false, false);
//static RegisterPass<Hello> X("hello", "Hello World Pass", false, false);
Pass *llvm::createHelloPass() {
	return new Hello();
}

namespace {
  // Hello2 - The second implementation with getAnalysisUsage implemented.
  struct Hello2 : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    Hello2() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      ++HelloCounter;
      //errs() << "Hello: ";
      errs().write_escaped(F.getName()) << '\n';
      return false;
    }

    // We don't modify the program, so we preserve all analyses.
    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesAll();
    }
  };
}

char Hello2::ID = 0;
static RegisterPass<Hello2> Z("hello2", "Hello World Pass (with getAnalysisUsage implemented)", false, false);

namespace {
	// Hello2 - The second implementation with getAnalysisUsage implemented.
	struct Hello3 : public ModulePass {
		static char ID; // Pass identification, replacement for typeid
		Hello3() : ModulePass(ID) {}

		bool runOnModule(Module &M) override {
			bool moduleModified = false;
			bool removeCurrentGlobal = false;

			std::vector<StringRef> foundStrings;
			std::vector<GlobalVariable*> globalRemoveList;

			// Get the function to call from our runtime library.
			// TODO: This will be the func that fills text
			LLVMContext& Ctx = M.getContext();
			/*Constant* fakeFunc = M.getOrInsertFunction(
				"fakeFunc", FunctionType::getVoidTy(Ctx), Type::getInt32Ty(Ctx), NULL
			);*/
			//Constant* fakeFunc = M.getFunction("fakeFunc");
			Constant* lookupFunc = M.getFunction("tableLookup");

			auto lookupTableExtern = M.getNamedValue("lookup_table");
			//lookupTableExtern->removeFromParent();
			

			for (auto& global : M.globals())
			{
				removeCurrentGlobal = false;

				errs() << "Global: ";
				errs() << global.getName();
				errs() << "\nType: ";
				errs() << global.getType();
				errs() << "\nUse Size: ";
				errs() << global.getNumUses();
				errs() << "\n";

				if (global.hasInitializer())
				{
					auto* constant = global.getInitializer();
					ConstantDataArray* data = dyn_cast<ConstantDataArray>(constant);
					if (data != nullptr)
					{
						auto* type = constant->getType();
						auto strData = data->getAsCString(); // OR getAsString
						errs() << "String: ";
						errs() << strData.str().c_str();
						errs() << "\n";

						for (auto i = global.use_begin(), e = global.use_end(); i != e; ++i)
						{
							auto user = i->getUser();
							user->dump();
							errs() << "\n";

							// TODO: remove duplicates...
							for (auto ci = user->use_begin(), ce = user->use_end(); ci != ce; ++ci)
							{
								//constExp->use
								auto constUser = ci->getUser();
								constUser->dump();
								//if(constUser->use)
								if (Instruction* constInst = dyn_cast<Instruction>(constUser))
								{
									constInst->dump();
									// Save the string. We only care about it if we get to this point...
									// TODO: Consider improving this
									foundStrings.push_back(strData);

									auto instParent = constInst->getParent();
									if (instParent != nullptr)
									{
										instParent->dump();
										errs() << "Building IR\n";
										IRBuilder<> builder(instParent);

										// Create an array
										ArrayType* arrayType = ArrayType::get(IntegerType::getInt8Ty(M.getContext()), strData.size());
										AllocaInst* allocInst = new AllocaInst(arrayType, 0, "strHolder");
										instParent->getInstList().push_front(allocInst);


										//allocInst->get
										//GetElementPtrInst::Create(IntegerType::getInt8PtrTy(M.getContext()), allocInst, )

										// Insert before this block
										// TODO: Is there a cleaner way to do this?
										builder.SetInsertPoint(instParent, --(--builder.GetInsertPoint()));

										// This indexList is necessary... need to find out why...
										Value* indexList[2] = { ConstantInt::get(IntegerType::getInt32Ty(M.getContext()), 0), ConstantInt::get(IntegerType::getInt32Ty(M.getContext()), 0) };
										auto createdRef = builder.CreateGEP(allocInst, indexList, "arrayRef");
										// TODO: Insert call to real fill function
										//builder.CreateCall(fakeFunc);

										// Use foundStrings size to determine the correct index
										Value* argList[2] = { createdRef, ConstantInt::get(IntegerType::getInt32Ty(M.getContext()), foundStrings.size() - 1) };
										builder.CreateCall(lookupFunc, argList);

										unsigned int count = constInst->getNumOperands();
										unsigned int opIndex = 0;
										while (opIndex < count)
										{
											auto currOp = constInst->getOperand(opIndex);
											currOp->dump();

											if (currOp == user)
											{
												errs() << "FOUND OP MATCH - REPLACING\n";
												constInst->setOperand(opIndex, createdRef);
											}

											++opIndex;
										}

										moduleModified = true;

										// Mark global for removal
										removeCurrentGlobal = true;
									}
								}
							}
						}
					}

					if (removeCurrentGlobal)
					{
						// Remove the global
						globalRemoveList.push_back(&global);
					}
				}
				else
				{
					// Remove the lookup_table var
					if (global.getName().equals("lookup_table"))
					{
						//globalRemoveList.push_back(&global);
					}
				}
			}

			// Need to do this outside of the foreach loop
			for (auto global : globalRemoveList)
			{
				global->eraseFromParent();
			}

			std::vector<Constant*> globalConsts;

			// Create the global data structure that will hold strings and the strings themselves
			for (auto str : foundStrings)
			{
				Constant *constString = ConstantDataArray::getString(M.global_begin()->getContext(), str, true /*addNull*/);
				GlobalVariable* globalStr = new GlobalVariable(/*Module=*/M,
					/*Type=*/constString->getType(),
					/*isConstant=*/true,
					/*Linkage=*/GlobalValue::PrivateLinkage,
					/*Initializer=*/constString,
					/*Name=*/".newStr");
				globalStr->setAlignment(1);

				Value* indexList[2] = { ConstantInt::get(IntegerType::getInt32Ty(M.getContext()), 0), ConstantInt::get(IntegerType::getInt32Ty(M.getContext()), 0) };
				auto constPtr = ConstantExpr::getGetElementPtr(globalStr->getInitializer()->getType(), globalStr, indexList, true);
				globalConsts.push_back(constPtr);
			}

			ArrayType* arrayType = ArrayType::get(IntegerType::getInt8PtrTy(M.getContext()), globalConsts.size());
			Constant *tableData = ConstantArray::get(arrayType, globalConsts);

			GlobalVariable* lookupTable = new GlobalVariable(/*Module=*/M,
				/*Type=*/tableData->getType(),
				/*isConstant=*/true,
				/*Linkage=*/GlobalValue::ExternalLinkage,
				/*Initializer=*/tableData, // set later
				/*Name=*/"lookup_table");
			lookupTable->setAlignment(4);

			// Use new table in lookup, remove old
			while (!lookupTableExtern->use_empty()) {
				auto &U = *lookupTableExtern->use_begin();
				U.set(lookupTable);
			}
			lookupTableExtern->eraseFromParent();

			for (NamedMDNode& meta : M.named_metadata())
			{
				errs() << meta.getName();
				errs() << "\n";
			}

			for (auto& func : M.functions())
			{
				errs() << func.getName();
				errs() << "\n";
				func.dump();
				errs() << "\n";

				for (auto& bb : func)
				{
					//IRBuilder<> builder(bb.getContext());
					//builder.CreateAlloca(Type::getInt8Ty(bb.getContext()), ConstantInt::get(IntegerType::getScalarType(), 10, false), "");
					for (auto& inst : bb)
					{
						errs() << "Instruction:\n";
						inst.dump();
						errs() << "\n";
						//auto* opList = inst.getOperandList();
						unsigned int count = inst.getNumOperands();
						unsigned int i = 0;
						while (i < count)
						{
							auto op = inst.getOperand(i);
							op->dump();
							errs() << "\n";

							if (ConstantExpr* constExp = dyn_cast<ConstantExpr>(op))
							{
								errs() << "Found constant expression";
								auto cEName = constExp->getOpcodeName();
								auto cName = constExp->getName();
								constExp->dump();
							}
							if (op->getName().contains(StringRef("str.4")))
							{
								errs() << "FOUND ANON STRING!";
							}
							
							i++;
						}
					}
				}
			}
			
			// Return true if module is modified
			return moduleModified;
		}

		// We don't modify the program, so we preserve all analyses.
		void getAnalysisUsage(AnalysisUsage &AU) const override {
			AU.setPreservesAll();
		}
	};
}

char Hello3::ID = 0;
static RegisterPass<Hello3> Y("hello3", "String pass", false, false);
