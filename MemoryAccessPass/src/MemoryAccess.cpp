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

#include <ChaoticIteration.h>
#include <MemoryAccess.h>

namespace MemoryAccessPass {

MemoryAccess::MemoryAccess() : llvm::FunctionPass(ID), visitor(new MemoryAccessInstVisitor()) {}

MemoryAccess::~MemoryAccess() {
	delete visitor;
}

bool MemoryAccess::runOnFunction(llvm::Function &F) {
	ChaoticIteration<MemoryAccessInstVisitor> chaoticIteration(*visitor);
	chaoticIteration.iterate(F);
	//visitor->visit(F);
	return false;
}
		
void MemoryAccess::print(llvm::raw_ostream &O, const llvm::Module *M) const {
	llvm::BasicBlock * last = &visitor->function->back();
	MemoryAccessData &data = visitor->data[last];
	O << "Stores to stack:\n";
	for (MemoryAccessData::StoreBaseToValuesMap::iterator it = data.stackStores.begin(),
								ie = data.stackStores.end();
			it != ie; it++) {
		const llvm::Value * pointer = it->first;
		O << "\t" << *pointer << "\n";
		MemoryAccessData::StoredValues & values = it->second;
		for (MemoryAccessData::StoredValues::iterator vit = values.begin(),
								vie = values.end();
				vit != vie; vit++) {
			O << "\t\t" << **vit << "\n";
		}
	}
	O << "Stores to globals:\n";
	for (MemoryAccessData::StoreBaseToValuesMap::iterator it = data.globalStores.begin(),
								ie = data.globalStores.end();
			it != ie; it++) {
		const llvm::Value * pointer = it->first;
		O << "\t" << *pointer << "\n";
		MemoryAccessData::StoredValues & values = it->second;
		for (MemoryAccessData::StoredValues::iterator vit = values.begin(),
								vie = values.end();
				vit != vie; vit++) {
			O << "\t\t" << **vit << "\n";
		}
	}
	O << "Stores to THE UNKNOWN:\n";
	for (MemoryAccessData::StoreBaseToValuesMap::iterator it = data.unknownStores.begin(),
								ie = data.unknownStores.end();
			it != ie; it++) {
		const llvm::Value * pointer = it->first;
		O << "\t" << *pointer << "\n";
		MemoryAccessData::StoredValues & values = it->second;
		for (MemoryAccessData::StoredValues::iterator vit = values.begin(),
								vie = values.end();
				vit != vie; vit++) {
			O << "\t\t" << **vit << "\n";
		}
	}
	O << "Function calls: Indirect: " << visitor->indirectFunctionCallCount << "\n";
	for (std::vector<const llvm::Function *>::iterator it = visitor->functionCalls.begin(),
			ie = visitor->functionCalls.end();
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

