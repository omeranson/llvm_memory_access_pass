#include <llvm/InstVisitor.h>
#include <llvm/IR/Instructions.h>

namespace MemoryAccessPass {
	class MemoryAccessInstVisitor : public llvm::InstVisitor<MemoryAccessInstVisitor> {
		private:
		public:
			void visitStoreInst(llvm::StoreInst &);
	};
}
