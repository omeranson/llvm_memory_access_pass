#ifndef MEMORY_ACCESS_INST_VISITOR_H
#define MEMORY_ACCESS_INST_VISITOR_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include <llvm/InstVisitor.h>
#include <llvm/IR/Instructions.h>

#include <MemoryAccessCache.h>
#include <ValueVisitor.h>

namespace MemoryAccessPass {
	extern int MemoryAccessGlobalAccessWatermark;
	extern int MemoryAccessArgumentAccessWatermark;
	extern int MemoryAccessFunctionCallCountWatermark;
	extern int VisitBlockCountWatermark;

	typedef enum {
		StoredValueTypeUnknown = 0,
		StoredValueTypePrimitive,
		StoredValueTypeConstant,
		StoredValueTypeStack,
		StoredValueTypeGlobal,
		StoredValueTypeHeap,
		StoredValueTypeArgument
	} StoredValueType;

	typedef enum {
		Tristate_Unknown,
		Tristate_False,
		Tristate_True
	} Tristate;


	struct StoredValue {
		llvm::Value * value;
		StoredValueType type;

		StoredValue() : value(0), type(StoredValueTypeUnknown) {}
		StoredValue(llvm::Value * a_value,
				const StoredValueType a_type) :
						value(a_value), type(a_type) {}
		StoredValue(const StoredValue & sv) :
				value(sv.value), type(sv.type) {}
		~StoredValue() {}
		bool operator==(const StoredValue & other) const {
			return ((value == other.value) && (type == other.type));
		}
		bool operator!=(const StoredValue & other) const {
			return !(*this == other);
		}
		void operator=(const StoredValue & other) {
			value = other.value;
			type = other.type;
		}
		bool operator<(const StoredValue & other) const {
			return (value < other.value);
		}
		bool isTop() const {
			return ((value == 0) && (type == StoredValueTypeUnknown));
		}
		static StoredValue top;
	};
	inline llvm::raw_ostream & operator<<(llvm::raw_ostream & O, const StoredValue & storedValue) {
		if (storedValue.isTop()) {
			O << "Top";
		} else {
			O << *(storedValue.value);
			switch (storedValue.type) {
			case StoredValueTypeUnknown:
				O << " (Unknown)";
				break;
			case StoredValueTypePrimitive:
				O << " (Primitive)";
				break;
			case StoredValueTypeConstant:
				O << " (Constant)";
				break;
			case StoredValueTypeStack:
				O << " (Stack)";
				break;
			case StoredValueTypeGlobal:
				O << " (Global)";
				break;
			case StoredValueTypeHeap:
				O << " (Heap)";
				break;
			case StoredValueTypeArgument:
				O << " (Argument)";
				break;
			}
		}
		return O;
	}

	typedef std::vector<StoredValue> StoredValues;
	typedef std::map<const llvm::Value*, StoredValue> StoreBaseToValueMap;

	class Evaluator : public llvm::ValueVisitor<Evaluator, StoredValue> {
	private:
		StoreBaseToValueMap & m_stores;
		std::vector<llvm::Instruction *> m_instsToDestroy;
	public:
		Evaluator(StoreBaseToValueMap & stores) : ValueVisitor<Evaluator, StoredValue>(), m_stores(stores) {}
		Evaluator(StoreBaseToValueMap & stores, StoreBaseToValueMap & cache) :
				ValueVisitor<Evaluator, StoredValue>(cache), m_stores(stores) {}
		~Evaluator() {
			for (std::vector<llvm::Instruction *>::iterator it = m_instsToDestroy.begin(),
									ie = m_instsToDestroy.end();
					it != ie; it++) {
				llvm::Instruction * inst = *it;
				delete inst;
				*it = 0;
			}
		}

		StoredValue visitInstruction(llvm::Instruction & instruction) {
			//llvm::errs() << __PRETTY_FUNCTION__ << ": " << instruction << ": Return top\n";
			StoredValue result(&instruction, StoredValueTypeUnknown);
			return result;
		}
		StoredValue visitArgument(llvm::Argument & argument);

		StoredValue visitValue(llvm::Value & value) {
			//llvm::errs() << __PRETTY_FUNCTION__ << ": " << value << ": Return top\n";
			StoredValue result(&value, StoredValueTypeUnknown);
			return result;
		}
		StoredValue visitConstantInt(llvm::ConstantInt & ci) {
			StoredValue result(&ci, StoredValueTypeConstant);
			return result;
		}
		StoredValue visitConstantFP(llvm::ConstantFP & cfp) {
			StoredValue result(&cfp, StoredValueTypeConstant);
			return result;
		}
		StoredValue visitConstant(llvm::Constant & constant) {
			//llvm::errs() << __PRETTY_FUNCTION__ << ": Return top\n";
			StoredValue result(&constant, StoredValueTypeUnknown);
			return result;
		}
		StoredValue visitConstantExpr(llvm::ConstantExpr & constantExpr);

		StoredValue visitGlobalValue(llvm::GlobalValue & globalValue);
		StoredValue visitAllocaInst(llvm::AllocaInst & allocaInst);
		StoredValue visitLoadInst(llvm::LoadInst & loadInst);
		StoredValue visitGetElementPtrInst(llvm::GetElementPtrInst & gepInst);
		StoredValue visitCastInst(llvm::CastInst & ci);
		StoredValue visitBinaryOperator(llvm::BinaryOperator & bo);
		StoredValue visitCallInst(llvm::CallInst & ci);
	};

	class MemoryAccessData {
	public:
		Evaluator m_evaluator;
		StoreBaseToValueMap stackStores;
		StoreBaseToValueMap globalStores;
		StoreBaseToValueMap argumentStores;
		StoreBaseToValueMap heapStores;
		StoreBaseToValueMap unknownStores;
		StoreBaseToValueMap temporaries;
		StoreBaseToValueMap stores;
		std::set<const llvm::CallInst *> functionCalls;
		std::set<const llvm::CallInst *> indirectFunctionCalls;
		//MemoryAccessData(MemoryAccessData& ); // TODO Copy constructor

		MemoryAccessData();
		~MemoryAccessData();
	};

	// Per function. For now.
	class MemoryAccessInstVisitor : public llvm::InstVisitor<MemoryAccessInstVisitor> {
	public:
		int visitBlockCount;
		bool haveIHadEnough;
		std::map<const llvm::BasicBlock*, MemoryAccessData*> data;
		llvm::Function * function;
		MemoryAccessData * functionData;
		mutable Tristate isSummariseFunctionCache;
		MemoryAccessInstVisitor();
		~MemoryAccessInstVisitor();
		MemoryAccessData & getData(const llvm::BasicBlock * bb);
		void runOnFunction(llvm::Function &, MemoryAccessCache * cache = 0);
		bool isSummariseFunction() const;
		void visitFunction(llvm::Function &);
		void visitBasicBlock(llvm::BasicBlock &);
		void visitCallInst(llvm::CallInst &);
		void visitStoreInst(llvm::StoreInst &);
		void store(MemoryAccessData & data, StoredValue & pointer, StoredValue & value);
		void join(MemoryAccessCache * cache = 0);
		bool join(const llvm::BasicBlock * from, const llvm::BasicBlock * to);
		bool join(const MemoryAccessData & from, MemoryAccessData & to) const;
		bool join(const StoreBaseToValueMap & from,
				StoreBaseToValueMap & to) const;
		template <class T>
		bool join(const std::set<const T *> & from,
				std::set<const T *> & to) const;
		bool joinCall(const llvm::CallInst & ci, MemoryAccessCache * cache);
		bool joinCalleeArguments(const llvm::CallInst & ci,
				const MemoryAccessInstVisitor * visitor);
		StoredValue joinStoredValues(StoreBaseToValueMap & stores,
				const llvm::Value * pointer, const StoredValue &value) const;
		void insertNoDups(
			StoredValues &fromValues,
			StoredValues & toValues) const;
	};
}
#endif // MEMORY_ACCESS_INST_VISITOR_H
