#ifndef MEMORY_ACCESS_INST_VISITOR_H
#define MEMORY_ACCESS_INST_VISITOR_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include <llvm/InstVisitor.h>
#include <llvm/IR/Instructions.h>

#include <ChaoticIteration.h>

namespace MemoryAccessPass {
	class MemoryAccessData {
	public:
		typedef std::set<const llvm::Value*> StoredValues;
		typedef std::map<const llvm::Value*, StoredValues> StoreBaseToValuesMap;
		StoreBaseToValuesMap stackStores;
		StoreBaseToValuesMap globalStores;
		StoreBaseToValuesMap unknownStores;
		//MemoryAccessData(MemoryAccessData& ); // Copy constructor
	};

	// Per function. For now.
	class MemoryAccessInstVisitor : public llvm::InstVisitor<MemoryAccessInstVisitor>, public Join {
	public:
		// Map function name to the memory access data
		std::map<const llvm::BasicBlock*, MemoryAccessData> data;
		std::vector<const llvm::Function *> functionCalls;
		llvm::Function * function;
		int indirectFunctionCallCount;
		MemoryAccessInstVisitor();
		void visitFunction(llvm::Function &);
		void visitCallInst(llvm::CallInst &);
		void visitStoreInst(llvm::StoreInst &);
		bool join(const llvm::BasicBlock * from, const llvm::BasicBlock * to);
		bool join(MemoryAccessData & from, MemoryAccessData & to);
		bool join(MemoryAccessData::StoreBaseToValuesMap & from,
				MemoryAccessData::StoreBaseToValuesMap & to);
	};
}
#endif // MEMORY_ACCESS_INST_VISITOR_H
