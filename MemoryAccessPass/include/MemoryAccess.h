#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>

#include <MemoryAccessInstVisitor.h>

namespace MemoryAccessPass {
	class MemoryAccess : public llvm::FunctionPass {
	private:
		MemoryAccessInstVisitor * visitor;
	public:
		static char ID;
		MemoryAccess();
		virtual ~MemoryAccess();
		virtual bool runOnFunction(llvm::Function &F);
		virtual void print(llvm::raw_ostream &O, const llvm::Module *M) const;
	};
}
