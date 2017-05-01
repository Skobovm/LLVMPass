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
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define DEBUG_TYPE "stringcompress"

STATISTIC(StringCompressCounter, "Counts number of functions greeted");

namespace {
  // Hello - The first implementation, without getAnalysisUsage.
  struct StringCompress : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
	StringCompress() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      ++HelloCounter;
      errs() << "Hello: ";
      //errs().write_escaped(F.getName()) << '\n';
      return false;
    }
  };
}

char StringCompress::ID = 0;
static RegisterPass<StringCompress> X("stringcompress", "String Compress Pass");

namespace {
  // Hello2 - The second implementation with getAnalysisUsage implemented.
  struct StringCompress2 : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
	StringCompress2() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      ++HelloCounter;
      errs() << "Hello: ";
      //errs().write_escaped(F.getName()) << '\n';
      return false;
    }

    // We don't modify the program, so we preserve all analyses.
    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesAll();
    }
  };
}

char StringCompress2::ID = 0;
static RegisterPass<StringCompress2> Y("stringcompress2", "String Compress Pass (with getAnalysisUsage implemented)");
