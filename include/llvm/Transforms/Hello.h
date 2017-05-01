#ifndef HELLO_PASS_H
#define HELLO_PASS_H

namespace llvm
{
	class BasicBlock;
	class BasicBlockPass;
	class Pass;

	Pass* createHelloPass();
}

#endif