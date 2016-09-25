#ifndef MEMORY_ACCESS_H
#define MEMORY_ACCESS_H
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
		void print(llvm::raw_ostream &O, const MemoryAccessData & data) const;
		void print(llvm::raw_ostream &O, const StoreBaseToValuesMap & stores) const;
		void print(llvm::raw_ostream &O, const std::map<const llvm::Value *, StoredValue> & temporaries) const;

		bool isSummariseFunction() const;
	};
}
#endif // MEMORY_ACCESS_H
