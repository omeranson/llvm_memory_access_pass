#include <MemoryAccessInstVisitor.h>
#include <llvm/Support/raw_ostream.h>

namespace MemoryAccessPass {

void MemoryAccessInstVisitor::visitStoreInst(llvm::StoreInst & si) {
	const llvm::Value * pointer = si.getPointerOperand();
	const llvm::Function * function = si.getParent()->getParent();
	llvm::errs() << "Store operation in function: " << function->getName() << " on pointer: " << *pointer << "\n";
	if (llvm::isa<llvm::AllocaInst>(pointer)) {
		llvm::errs() << "\tIs Stack.\n";
	} else if (llvm::isa<llvm::GlobalValue>(pointer)) {
		llvm::errs() << "\tIs global.\n";
	} else {
		llvm::errs() << "\tDunno.\n";
	}
}

}
