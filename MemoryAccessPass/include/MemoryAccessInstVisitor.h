#ifndef MEMORY_ACCESS_INST_VISITOR_H
#define MEMORY_ACCESS_INST_VISITOR_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include <llvm/InstVisitor.h>
#include <llvm/IR/Instructions.h>

#include <ValueVisitor.h>

namespace MemoryAccessPass {

	typedef enum {
		StoredValueTypeStack = 0,
		StoredValueTypeGlobal,
		StoredValueTypeHeap,
		StoredValueTypeUnknown
	} StoredValueType;

	struct StoredValue {
		const llvm::Value * value;
		StoredValueType type;

		StoredValue() : value(0), type(StoredValueTypeUnknown) {}
		StoredValue(const llvm::Value * a_value,
				const StoredValueType a_type) :
						value(a_value), type(a_type) {}
		StoredValue(const StoredValue & sv) :
				value(sv.value), type(sv.type) {}
		~StoredValue() {}
		bool operator==(const StoredValue & other) const {
			return ((value == other.value) && (type == other.type));
		}
		void operator=(const StoredValue & other) {
			value = other.value;
			type = other.type;
		}
		bool operator<(const StoredValue & other) {
			return (value < other.value);
		}
	};

	typedef std::vector<StoredValue*> StoredValues;
	typedef std::map<const llvm::Value*, StoredValues> StoreBaseToValuesMap;

	class Evaluator : public llvm::ValueVisitor<Evaluator, StoredValue*> {
	public:
		Evaluator() : ValueVisitor<Evaluator, StoredValue*>() {}
		Evaluator(std::map<const llvm::Value *, StoredValue*> & cache) :
				ValueVisitor<Evaluator, StoredValue*>(cache) {}
		~Evaluator() {}

		StoredValue* visitInstruction(llvm::Instruction & value) { return new StoredValue(); }
		StoredValue* visitArgument(llvm::Argument & argument) { return new StoredValue(); }
		StoredValue* visitValue(llvm::Value & value) { return new StoredValue(); }
		StoredValue* visitConstantInt(llvm::ConstantInt & ci) { return new StoredValue(); }
		StoredValue* visitConstantFP(llvm::ConstantFP & cfp) { return new StoredValue(); }
		StoredValue* visitConstant(llvm::Constant & constant) { return new StoredValue(); }

		StoredValue* visitGlobalValue(llvm::GlobalValue & globalValue);
		StoredValue* visitAllocaInst(llvm::AllocaInst & allocaInst);
	};

	class MemoryAccessData {
	public:
		Evaluator m_evaluator;
		StoreBaseToValuesMap stackStores;
		StoreBaseToValuesMap globalStores;
		StoreBaseToValuesMap unknownStores;
		std::map<const llvm::Value *, StoredValue*> temporaries;
		//MemoryAccessData(MemoryAccessData& ); // TODO Copy constructor

		MemoryAccessData();
		~MemoryAccessData();
	};

	// Per function. For now.
	class MemoryAccessInstVisitor : public llvm::InstVisitor<MemoryAccessInstVisitor> {
	public:
		std::map<const llvm::BasicBlock*, MemoryAccessData*> data;
		std::vector<const llvm::Function *> functionCalls;
		llvm::Function * function;
		int indirectFunctionCallCount;
		MemoryAccessInstVisitor();
		~MemoryAccessInstVisitor();
		MemoryAccessData & getData(const llvm::BasicBlock * bb);
		void visitFunction(llvm::Function &);
		void visitCallInst(llvm::CallInst &);
		void visitStoreInst(llvm::StoreInst &);
		bool join(const llvm::BasicBlock * from, const llvm::BasicBlock * to);
		bool join(const MemoryAccessData & from, MemoryAccessData & to) const;
		bool join(const StoreBaseToValuesMap & from,
				StoreBaseToValuesMap & to) const;
		void insertNoDups(
			StoredValues &fromValues,
			StoredValues & toValues) const;
	};
}
#endif // MEMORY_ACCESS_INST_VISITOR_H
