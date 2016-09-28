#ifndef MEMORY_ACCESS_H
#define MEMORY_ACCESS_H
#include <map>

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>

#include <MemoryAccessInstVisitor.h>
#include <MemoryAccessCache.h>

namespace MemoryAccessPass {
	class MemoryAccess : public llvm::FunctionPass {
	private:
		MemoryAccessInstVisitor * lastVisitor;
		std::map<llvm::Function *, MemoryAccessInstVisitor *> visitors;
		MemoryAccessInstVisitor * getModifiableVisitor(llvm::Function *F);
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
		const MemoryAccessData * getSummaryData() const;
		const MemoryAccessInstVisitor * getVisitor(llvm::Function *F);
	};
}
#endif // MEMORY_ACCESS_H
