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
#include <MemoryAccessInstVisitor.h>

namespace MemoryAccessPass {

MemoryAccess::MemoryAccess() : llvm::FunctionPass(ID) {}

MemoryAccess::~MemoryAccess() {}

bool MemoryAccess::runOnFunction(llvm::Function &F) {
	llvm::errs() << "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n";
	llvm::errs() << "MemoryAccess: Function: ";
	llvm::errs().write_escaped(F.getName()) << '\n';
	MemoryAccessInstVisitor visitor;
	visitor.visit(F);
	return false;
}
		
void MemoryAccess::print(llvm::raw_ostream &O, const llvm::Module *M) const {
	// TODO Print here the information gathered.
	// O << "MemoryAccess analysis for functions\n";
}

char MemoryAccess::ID = 0;
static llvm::RegisterPass<MemoryAccess> _X(
		"memaccess",
		"Output information about function's memory access",
		false, false);
}

