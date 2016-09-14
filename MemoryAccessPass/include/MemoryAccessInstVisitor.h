#ifndef MEMORY_ACCESS_INST_VISITOR_H
#define MEMORY_ACCESS_INST_VISITOR_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include <llvm/InstVisitor.h>
#include <llvm/IR/Instructions.h>

namespace MemoryAccessPass {
	class MemoryAccessData {
	public:
		typedef std::vector<const llvm::Value*> StoredValues;
		typedef std::map<const llvm::Value*, StoredValues> StoreBaseToValuesMap;
		StoreBaseToValuesMap stackStores;
		StoreBaseToValuesMap globalStores;
		StoreBaseToValuesMap unknownStores;
		//MemoryAccessData(MemoryAccessData& ); // TODO Copy constructor
	};

	// Per function. For now.
	class MemoryAccessInstVisitor : public llvm::InstVisitor<MemoryAccessInstVisitor> {
	public:
		std::map<const llvm::BasicBlock*, MemoryAccessData> data;
		std::vector<const llvm::Function *> functionCalls;
		llvm::Function * function;
		int indirectFunctionCallCount;
		MemoryAccessInstVisitor();
		void visitFunction(llvm::Function &);
		void visitCallInst(llvm::CallInst &);
		void visitStoreInst(llvm::StoreInst &);
		bool join(const llvm::BasicBlock * from, const llvm::BasicBlock * to);
		bool join(MemoryAccessData & from, MemoryAccessData & to) const;
		bool join(MemoryAccessData::StoreBaseToValuesMap & from,
				MemoryAccessData::StoreBaseToValuesMap & to) const;
		void insertNoDups(
			MemoryAccessData::StoredValues &fromValues,
			MemoryAccessData::StoredValues & toValues) const;
	};
}
#endif // MEMORY_ACCESS_INST_VISITOR_H
