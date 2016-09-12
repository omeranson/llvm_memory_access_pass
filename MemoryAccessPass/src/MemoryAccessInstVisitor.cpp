#include <MemoryAccessInstVisitor.h>
#include <llvm/Support/raw_ostream.h>

namespace MemoryAccessPass {

MemoryAccessInstVisitor::MemoryAccessInstVisitor() :
		current(0), llvm::InstVisitor<MemoryAccessInstVisitor>() {}

void MemoryAccessInstVisitor::visitFunction(llvm::Function &function) {
	std::string functionName = function.getName().str();
	std::pair<std::map<std::string, MemoryAccessData>::iterator, bool> insert_result = data.insert(
			std::make_pair(functionName, MemoryAccessData()));
	if (insert_result.second == false) {
		llvm::errs() << "Function " << functionName << " already analysed\n";
		current = 0;
	} else {
		current = &(insert_result.first->second);
	}
}

void MemoryAccessInstVisitor::visitStoreInst(llvm::StoreInst & si) {
	if (!current) {
		llvm::errs() << "ERROR: Currently analysed function unknown\n";
		return;
	}
	const llvm::Value * pointer = si.getPointerOperand();
	const llvm::Function * function = si.getParent()->getParent();
	if (llvm::isa<llvm::AllocaInst>(pointer)) {
		current->stackStores.push_back(&si);
	} else if (llvm::isa<llvm::GlobalValue>(pointer)) {
		current->globalStores.push_back(&si);
	} else {
		current->unknownStores.push_back(&si);
	}
}

void MemoryAccessInstVisitor::visitCallInst(llvm::CallInst & ci) {
	const llvm::Function * callee = ci.getCalledFunction();
	if (callee) {
		current->functionCalls.push_back(callee);
	} else {
		++current->indirectFunctionCallCount;
		llvm::errs() << "Indirect function call: " <<
				*(ci.getCalledValue()) << "\n";
	}
}

}
