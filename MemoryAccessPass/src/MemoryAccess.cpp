#include <cassert>
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
#include <llvm/DebugInfo.h>
#include <llvm/IR/Constants.h>

#include <MemoryAccess.h>
#include <MemoryAccessInstVisitor.h>

namespace MemoryAccessPass {

const char * predefinedFunctions[] = {
	"__assert_fail",
	"exit",
	"_exit",
	"malloc",
	"realloc",
	"free",
	0
};

bool isPredefinedFunction(llvm::Function & F) {
	llvm::StringRef name = F.getName();
	if (name.startswith("klee_")) {
		return true;
	}
	if (name.startswith("__cxa")) {
		return true;
	}
	if (name.startswith("__cxx")) {
		return true;
	}
	for (int idx = 0; predefinedFunctions[idx]; idx++) {
		if (name.equals(predefinedFunctions[idx])) {
			return true;
		}
	}
	return false;
}


MemoryAccess::MemoryAccess() :
		llvm::FunctionPass(ID), lastVisitor(0) {}

MemoryAccess::~MemoryAccess() {
	for (std::map<llvm::Function *, MemoryAccessInstVisitor *>::iterator
			it = visitors.begin(), ie = visitors.end();
			it != ie; it++) {
		delete it->second;
		it->second = 0;
	}
}

bool MemoryAccess::runOnFunction(llvm::Function &F) {
	lastVisitor = getModifiableVisitor(&F);
	return false;
}

bool MemoryAccess::isSummariseFunction() const {
	assert(lastVisitor && "isSummariseFunction called before runOnFunction");
	return lastVisitor->isSummariseFunction();
}

const MemoryAccessData * MemoryAccess::getSummaryData() const {
	assert(lastVisitor && "isSummariseFunction called before runOnFunction");
	return lastVisitor->functionData;
}

MemoryAccessInstVisitor * MemoryAccess::getModifiableVisitor(llvm::Function *F) {
	MemoryAccessInstVisitor * visitor = visitors[F];
	if (!visitor) {
		visitor = new MemoryAccessInstVisitor();
		visitors[F] = visitor;
		MemoryAccessCacheDuck<MemoryAccess> cache(*this);
		visitor->runOnFunction(*F, &cache);
	}
	return visitor;
}

const MemoryAccessInstVisitor * MemoryAccess::getVisitor(llvm::Function *F) {
	return getModifiableVisitor(F);
}

void MemoryAccess::print(llvm::raw_ostream &O, const StoreBaseToValueMap & stores) const {
	for (StoreBaseToValueMap::const_iterator it = stores.begin(),
							ie = stores.end();
			it != ie; it++) {
		const llvm::Value * pointer = it->first;
		O << "\t>" << *pointer  << " <- " << it->second << "\n";
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
	O << "Function calls: Indirect: " << data.indirectFunctionCalls.size() << " Direct:\n";
	for (std::set<const llvm::CallInst *>::iterator it = data.functionCalls.begin(),
								ie = data.functionCalls.end();
			it != ie; it++) {
		const llvm::CallInst * ci = *it;
		const llvm::Function * function = ci->getCalledFunction();
		O << "\t>" << function->getName() << "\n";
	}
	O << "Temporaries:\n";
	print(O, data.temporaries);
	O << "Stores:\n";
	print(O, data.stores);
	O << "Is summarise: " << isSummariseFunction() << "\n";
}

void MemoryAccess::print(llvm::raw_ostream &O, const llvm::Module *M) const {
	print(O, *lastVisitor->functionData);
}

char MemoryAccess::ID = 0;
static llvm::RegisterPass<MemoryAccess> _X(
		"memaccess",
		"Output information about function's memory access",
		false, false);
}

