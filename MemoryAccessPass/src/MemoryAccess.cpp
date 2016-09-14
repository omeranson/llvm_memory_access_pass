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
		
void MemoryAccess::print(llvm::raw_ostream &O, const MemoryAccessData::StoreBaseToValuesMap & stores) const {
	for (MemoryAccessData::StoreBaseToValuesMap::const_iterator it = stores.begin(),
									ie = stores.end();
			it != ie; it++) {
		const llvm::Value * pointer = it->first;
		O << "\t>" << *pointer << "\n";
		const MemoryAccessData::StoredValues & values = it->second;
		for (MemoryAccessData::StoredValues::const_iterator vit = values.begin(),
									vie = values.end();
				vit != vie; vit++) {
			O << "\t\t>" << **vit << "\n";
		}
	}
}

void MemoryAccess::print(llvm::raw_ostream &O, const MemoryAccessData & data) const {
	O << "Stores to stack:\n";
	print(O, data.stackStores);
	O << "Stores to globals:\n";
	print(O, data.globalStores);
	O << "Stores to THE UNKNOWN:\n";
	print(O, data.unknownStores);
}

void MemoryAccess::print(llvm::raw_ostream &O, const llvm::Module *M) const {
	MemoryAccessData data;
	for (llvm::Function::iterator it = visitor->function->begin(),
				ie = visitor->function->end();
			it != ie; it++) {
		MemoryAccessData &bb_data = visitor->data[it];
		visitor->join(bb_data, data);
	}
	print(O, data);
	O << "Function calls: Indirect: " << visitor->indirectFunctionCallCount << "\n";
	for (std::vector<const llvm::Function *>::iterator it = visitor->functionCalls.begin(),
								ie = visitor->functionCalls.end();
			it != ie; it++) {
		const llvm::Function * function = *it;
		O << "\t>" << function->getName() << "\n";
	}
}

char MemoryAccess::ID = 0;
static llvm::RegisterPass<MemoryAccess> _X(
		"memaccess",
		"Output information about function's memory access",
		false, false);
}

