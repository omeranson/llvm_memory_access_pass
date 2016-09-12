#include <list>
#include <set>
#include <map>
#include <string>
#include <iostream>
#include <sstream>

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstrTypes.h>
//#include <llvm/IR/DebugLoc.h>
#include <llvm/DebugInfo.h>
#include <llvm/IR/Constants.h>

#include <MemoryAccess.h>

namespace MemoryAccessPass {

MemoryAccess::MemoryAccess() : llvm::FunctionPass(ID) {}

MemoryAccess::~MemoryAccess() {
	delete visitor;
}

bool MemoryAccess::runOnFunction(llvm::Function &F) {
	visitor = new MemoryAccessInstVisitor();
	visitor->visit(F);
	return false;
}
		
void MemoryAccess::print(llvm::raw_ostream &O, const llvm::Module *M) const {
	MemoryAccessData &data = *(visitor->current);
	O << "Stores to stack:\n";
	for (std::vector<const llvm::Instruction *>::iterator it = data.stackStores.begin(),
			ie = data.stackStores.end();
			it != ie; it++) {
		const llvm::StoreInst * si = llvm::cast<llvm::StoreInst>(*it);
		const llvm::Value * pointer = si->getPointerOperand();
		O << "\t" << *pointer << "\n";
	}
	O << "Stores to globals:\n";
	for (std::vector<const llvm::Instruction *>::iterator it = data.globalStores.begin(),
			ie = data.globalStores.end();
			it != ie; it++) {
		const llvm::StoreInst * si = llvm::cast<llvm::StoreInst>(*it);
		const llvm::Value * pointer = si->getPointerOperand();
		O << "\t" << *pointer << "\n";
	}
	O << "Stores to THE UNKNOWN:\n";
	for (std::vector<const llvm::Instruction *>::iterator it = data.unknownStores.begin(),
			ie = data.unknownStores.end();
			it != ie; it++) {
		const llvm::StoreInst * si = llvm::cast<llvm::StoreInst>(*it);
		const llvm::Value * pointer = si->getPointerOperand();
		O << "\t" << *pointer << "\n";
	}
	O << "Function calls: Indirect: " << data.indirectFunctionCallCount << "\n";
	for (std::vector<const llvm::Function *>::iterator it = data.functionCalls.begin(),
			ie = data.functionCalls.end();
			it != ie; it++) {
		const llvm::Function * function = *it;
		O << "\t" << function->getName() << "\n";
	}
}

char MemoryAccess::ID = 0;
static llvm::RegisterPass<MemoryAccess> _X(
		"memaccess",
		"Output information about function's memory access",
		false, false);
}

