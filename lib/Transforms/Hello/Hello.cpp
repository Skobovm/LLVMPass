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
#include "llvm/Analysis/CaptureTracking.h"

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

		std::map<StringRef, unsigned int> wordMap;
		unsigned int wordIndex = 0;

		// Splits the string into space-delimited tokens and inserts into wordMap, returning word indices that
		// are used to call the string lookup function
		std::vector<unsigned int> getComponentsFromString(StringRef text)
		{
			std::vector<unsigned int> retVector;

			while (!text.empty())
			{
				std::pair<StringRef, StringRef> splitText = text.split(" ");

				// Check wordMap for first part
				auto search = wordMap.find(splitText.first);
				if (search != wordMap.end())
				{
					// Found this word, use its index
					retVector.push_back(search->second);
				}
				else
				{
					// Did not find this word, insert into table
					wordMap.insert({ splitText.first, wordIndex });
					retVector.push_back(wordIndex);
					wordIndex++;
				}

				// Keep splitting if we have more
				text = splitText.second;
			}
			return retVector;
		}

		bool runOnModule(Module &M) override {
			bool moduleModified = false;
			bool removeCurrentGlobal = false;
			int currentWordIndex = 0;

			std::vector<StringRef> foundStrings;
			std::vector<GlobalVariable*> globalRemoveList;

			// Get the function to call from our runtime library.
			// This will be the func that fills text
			LLVMContext& Ctx = M.getContext();

			// Leave for reference
			//Constant* lookupFunc = M.getFunction("tableLookup");
			

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
						// Sloppy AF. Refactor later
						if (!data->isCString())
						{
							continue;
						}

						auto strData = data->getAsCString(); // OR getAsString
						errs() << "String: ";
						errs() << strData.str().c_str();
						errs() << "\n";

						for (auto i = global.use_begin(), e = global.use_end(); i != e; ++i)
						{
							auto user = i->getUser();
							user->dump();
							errs() << "\n";

							for (auto ci = user->user_begin(), ce = user->user_end(); ci != ce; ++ci)
							{
								auto constUser = ci;
								if (auto constInst = dyn_cast<Instruction>(*constUser))
								{
									constInst->dump();

									// Check for capture on user - it is the pointer referencing the data
									//bool pointerMayBeCaptured = PointerMayBeCaptured(user, false, true);

									// Hacky way to not create duplicates...
									unsigned int count = constInst->getNumOperands();
									unsigned int opIndex = 0;
									bool foundUse = false;
									while (opIndex < count)
									{
										auto currOp = constInst->getOperand(opIndex);
										if (currOp == user)
										{
											foundUse = true;
										}
										++opIndex;
									}
									if (!foundUse)
									{
										break;
									}

									std::vector<unsigned int> wordComponents = getComponentsFromString(strData);
									// Save the string. We only care about it if we get to this point...
									// TODO: Consider improving this
									foundStrings.push_back(strData);

									auto instParent = constInst->getParent();
									if (instParent != nullptr)
									{										
										errs() << "Building IR\n";
										IRBuilder<> builder(instParent);

										// Create an array to hold the word
										ArrayType* arrayType = ArrayType::get(IntegerType::getInt8Ty(M.getContext()), strData.size());
										AllocaInst* allocInst = new AllocaInst(arrayType, 0, "strHolder");
										instParent->getInstList().push_front(allocInst);

										// TODO: These function calls should be next to the function that uses them, rather than next to the alloc
										// Insert immediately before instruction that uses the string
										auto nextNode = allocInst->getNextNode();
										builder.SetInsertPoint(nextNode);

										// Find the nearest debug location
										DebugLoc debugLoc;
										for (auto beginInst = instParent->getInstList().begin(), endInst = instParent->getInstList().end(); beginInst != endInst; beginInst++)
										{
											auto currLoc = beginInst->getDebugLoc();
											if (currLoc.get() != nullptr)
											{
												debugLoc = currLoc;
												break;
											}
										}
										builder.SetCurrentDebugLocation(debugLoc);

										// Create array to hold the word indices
										Constant *wordIndexArray = ConstantDataArray::get(M.getContext(), wordComponents); 
										GlobalVariable* wordIndexVar = new GlobalVariable(/*Module=*/M,
											/*Type=*/wordIndexArray->getType(),
											/*isConstant=*/true,
											/*Linkage=*/GlobalValue::InternalLinkage,
											/*Initializer=*/wordIndexArray,
											/*Name=*/".wordIndexGlobal");
										wordIndexVar->setAlignment(4);

										// This indexList is necessary... need to find out why...
										Value* indexList[2] = { ConstantInt::get(IntegerType::getInt32Ty(M.getContext()), 0), ConstantInt::get(IntegerType::getInt32Ty(M.getContext()), 0) };
										auto createdRef = builder.CreateGEP(allocInst, indexList, "arrayRef");
										auto wordIndexPtr = builder.CreateGEP(wordIndexVar, indexList, "wordIndexRef");

										// This was the old table type. Leave for future reference
										// Use foundStrings size to determine the correct index
										//Value* argList[2] = { createdRef, ConstantInt::get(IntegerType::getInt32Ty(M.getContext()), foundStrings.size() - 1) };
										//builder.CreateCall(lookupFunc, argList);
										
										// Use wordIndexVar to create a call to compressed string lookup func
										Constant* lookupFuncCompressed = M.getFunction("tableLookupSpace");
										Value* argListCompressed[3] = { createdRef, wordIndexPtr, ConstantInt::get(IntegerType::getInt32Ty(M.getContext()), wordComponents.size()) };
										auto callInst = builder.CreateCall(lookupFuncCompressed, argListCompressed);

										callInst->dump();
										instParent->dump();

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

			// This is the old table type. Leave for future reference!

			//std::vector<Constant*> globalConsts;
			
			//// Create the global data structure that will hold strings and the strings themselves
			//for (auto str : foundStrings)
			//{
			//	Constant *constString = ConstantDataArray::getString(M.global_begin()->getContext(), str, true /*addNull*/);
			//	GlobalVariable* globalStr = new GlobalVariable(/*Module=*/M,
			//		/*Type=*/constString->getType(),
			//		/*isConstant=*/true,
			//		/*Linkage=*/GlobalValue::PrivateLinkage,
			//		/*Initializer=*/constString,
			//		/*Name=*/".newStr");
			//	globalStr->setAlignment(1);

			//	Value* indexList[2] = { ConstantInt::get(IntegerType::getInt32Ty(M.getContext()), 0), ConstantInt::get(IntegerType::getInt32Ty(M.getContext()), 0) };
			//	auto constPtr = ConstantExpr::getGetElementPtr(globalStr->getInitializer()->getType(), globalStr, indexList, true);
			//	globalConsts.push_back(constPtr);
			//}

			//ArrayType* arrayType = ArrayType::get(IntegerType::getInt8PtrTy(M.getContext()), globalConsts.size());
			//Constant *tableData = ConstantArray::get(arrayType, globalConsts);

			//GlobalVariable* lookupTable = new GlobalVariable(/*Module=*/M,
			//	/*Type=*/tableData->getType(),
			//	/*isConstant=*/true,
			//	/*Linkage=*/GlobalValue::ExternalLinkage,
			//	/*Initializer=*/tableData, // set later
			//	/*Name=*/"lookup_table");
			//lookupTable->setAlignment(4);


			// Create the compressed string lookup table
			std::vector<Constant*> compressedGlobalConsts;

			// Insert the words in order
			std::vector<StringRef> compressedWords(wordMap.size());
			for (auto word : wordMap)
			{
				compressedWords[word.second] = word.first;
			}

			for (auto str : compressedWords)
			{
				Constant *constString = ConstantDataArray::getString(M.global_begin()->getContext(), str, true /*addNull*/);
				GlobalVariable* globalStr = new GlobalVariable(/*Module=*/M,
					/*Type=*/constString->getType(),
					/*isConstant=*/true,
					/*Linkage=*/GlobalValue::PrivateLinkage,
					/*Initializer=*/constString,
					/*Name=*/".compStr");
				globalStr->setAlignment(1);

				Value* indexList[2] = { ConstantInt::get(IntegerType::getInt32Ty(M.getContext()), 0), ConstantInt::get(IntegerType::getInt32Ty(M.getContext()), 0) };
				auto constPtr = ConstantExpr::getGetElementPtr(globalStr->getInitializer()->getType(), globalStr, indexList, true);
				compressedGlobalConsts.push_back(constPtr);
			}

			ArrayType* arrType = ArrayType::get(IntegerType::getInt8PtrTy(M.getContext()), compressedGlobalConsts.size());
			Constant *compressedData = ConstantArray::get(arrType, compressedGlobalConsts);

			GlobalVariable* compressedLookupTable = new GlobalVariable(/*Module=*/M,
				/*Type=*/compressedData->getType(),
				/*isConstant=*/true,
				/*Linkage=*/GlobalValue::ExternalLinkage,
				/*Initializer=*/compressedData, // set later
				/*Name=*/"lookup_table_compressed");
			compressedLookupTable->setAlignment(4);

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
			}

			for (auto& func : M.functions())
			{
				errs() << func.getName();
				errs() << "\n";
				func.dump();
				errs() << "\n";

				for (auto& bb : func)
				{
					for (auto& inst : bb)
					{
						errs() << "Instruction:\n";
						inst.dump();
						errs() << "\n";

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
