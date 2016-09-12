#include <vector>
#include <map>
#include <string>

#include <llvm/InstVisitor.h>
#include <llvm/IR/Instructions.h>

namespace MemoryAccessPass {
	class MemoryAccessData {
	public:
		std::vector<const llvm::Instruction *> stackStores;
		std::vector<const llvm::Instruction *> globalStores;
		std::vector<const llvm::Instruction *> unknownStores;
		std::vector<const llvm::Function *> functionCalls;
		int indirectFunctionCallCount;
		//MemoryAccessData(MemoryAccessData& ); // Copy constructor
	};

	class MemoryAccessInstVisitor : public llvm::InstVisitor<MemoryAccessInstVisitor> {
	public:
		// Map function name to the memory access data
		std::map<std::string, MemoryAccessData> data;
		MemoryAccessData * current;
		MemoryAccessInstVisitor();
		void visitFunction(llvm::Function &);
		void visitCallInst(llvm::CallInst &);
		void visitStoreInst(llvm::StoreInst &);
	};
}
