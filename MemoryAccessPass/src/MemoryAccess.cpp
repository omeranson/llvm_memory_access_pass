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
#include <MemoryAccessInstVisitor.h>

namespace MemoryAccessPass {

int MemoryAccessGlobalAccessWatermark = 10;
int MemoryAccessFunctionCallCountWatermark = 10;

MemoryAccess::MemoryAccess() :
		llvm::FunctionPass(ID), visitor(0) {}

MemoryAccess::~MemoryAccess() {
	delete visitor;
}

bool MemoryAccess::runOnFunction(llvm::Function &F) {
	delete visitor;
	visitor = new MemoryAccessInstVisitor();
	ChaoticIteration<MemoryAccessInstVisitor> chaoticIteration(*visitor);
	chaoticIteration.iterate(F);
	visitor->join();
	return false;
}

bool MemoryAccess::isSummariseFunction() {
	if (visitor->functionData->indirectFunctionCalls.size() > 0) {
		return false;
	}
	if (visitor->functionData->unknownStores.size() > 0) {
		return false;
	}
	if (visitor->functionData->heapStores.size() > 0) {
		return false;
	}
	if (visitor->functionData->argumentStores.size() > 0) {
		return false;
	}
	if (visitor->functionData->globalStores.size() >= MemoryAccessGlobalAccessWatermark) {
		return false;
	}
	if (visitor->functionData->functionCalls.size() >= MemoryAccessFunctionCallCountWatermark) {
		return false;
	}
	return true;
}

void MemoryAccess::print(llvm::raw_ostream &O, const StoreBaseToValuesMap & stores) const {
	for (StoreBaseToValuesMap::const_iterator it = stores.begin(),
							ie = stores.end();
			it != ie; it++) {
		const llvm::Value * pointer = it->first;
		O << "\t>" << *pointer << "\n";
		const StoredValues & values = it->second;
		for (StoredValues::const_iterator vit = values.begin(),
							vie = values.end();
				vit != vie; vit++) {
			const StoredValue & storedValue = *vit;
			O << "\t\t>" << storedValue << "\n";
		}
	}
}

void MemoryAccess::print(llvm::raw_ostream &O, const std::map<const llvm::Value *, StoredValue> & temporaries) const {
	for (std::map<const llvm::Value *, StoredValue>::const_iterator it = temporaries.begin(),
									ie = temporaries.end();
			it != ie; it++) {
		const llvm::Value * pointer = it->first;
		O << "\t>" << *pointer << "\n";
		const StoredValue & value = it->second;
		O << "\t\t>" << value << "\n";
	}
}

void MemoryAccess::print(llvm::raw_ostream &O, const MemoryAccessData & data) const {
	O << "Stores to stack:\n";
	print(O, data.stackStores);
	O << "Stores to globals:\n";
	print(O, data.globalStores);
	O << "Stores to argument pointers:\n";
	print(O, data.argumentStores);
	O << "Stores to the heap:\n";
	print(O, data.heapStores);
	O << "Stores to THE UNKNOWN:\n";
	print(O, data.unknownStores);
	O << "Temporaries:\n";
	print(O, data.temporaries);
	O << "Stores:\n";
	print(O, data.stores);
	O << "Function calls: Indirect: " << data.indirectFunctionCalls.size() << " Direct:\n";
	for (std::set<const llvm::Function *>::iterator it = data.functionCalls.begin(),
								ie = data.functionCalls.end();
			it != ie; it++) {
		const llvm::Function * function = *it;
		O << "\t>" << function->getName() << "\n";
	}
}

void MemoryAccess::print(llvm::raw_ostream &O, const llvm::Module *M) const {
	print(O, *visitor->functionData);
}

char MemoryAccess::ID = 0;
static llvm::RegisterPass<MemoryAccess> _X(
		"memaccess",
		"Output information about function's memory access",
		false, false);
}

