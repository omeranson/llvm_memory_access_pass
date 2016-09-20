#include <algorithm>

#include <llvm/Support/raw_ostream.h>

#include <MemoryAccessInstVisitor.h>

namespace MemoryAccessPass {

StoredValue StoredValue::top = StoredValue();

StoredValue Evaluator::visitGlobalValue(llvm::GlobalValue & globalValue) {
	const llvm::Value * value = &globalValue;
	StoredValue result(value, StoredValueTypeGlobal);
	m_cache.insert(std::make_pair(value, result));
	return result;
}

StoredValue Evaluator::visitAllocaInst(llvm::AllocaInst & allocaInst) {
	const llvm::Value * value = &allocaInst;
	StoredValue result(value, StoredValueTypeStack);
	m_cache.insert(std::make_pair(value, result));
	return result;
}

StoredValue Evaluator::visitLoadInst(llvm::LoadInst & loadInst) {
	llvm::Value * pointer = loadInst.getPointerOperand();
	StoreBaseToValueMap::iterator it = m_stores.find(pointer);
	if (it != m_stores.end()) {
		StoredValue result = m_stores[pointer];
		return result;
	}
	StoredValueType type = (loadInst.getType()->isPointerTy()) ?
			StoredValueTypeUnknown : StoredValueTypePrimitive;
	StoredValue result(&loadInst, type);
	return result;
}

StoredValue Evaluator::visitGetElementPtrInst(llvm::GetElementPtrInst & gepInst) {
	bool isConstParams = true;
	llvm::Value * pointer = gepInst.getPointerOperand();
	StoredValue pointerStoredValue = visit(pointer);
	for (llvm::User::op_iterator it = gepInst.idx_begin(),
					ie = gepInst.idx_end();
			it != ie; it++) {
		llvm::Value * operand = *it;
		StoredValue operandStoredValue = visit(operand);
		if (operandStoredValue.type != StoredValueTypeConstant) {
			isConstParams = false;
			break;
		}
	}
	StoredValueType pointerStoredValueType = isConstParams ?
			pointerStoredValue.type : StoredValueTypeUnknown;
	StoredValue result(&gepInst, pointerStoredValueType);
	if (isConstParams) {
		m_cache.insert(std::make_pair(&gepInst, result));
	}
	return result;
}

StoredValue Evaluator::visitArgument(llvm::Argument & argument) {
	StoredValueType type = argument.getType()->isPointerTy() ?
			StoredValueTypeArgument : StoredValueTypePrimitive;
	StoredValue result(&argument, type);
	m_cache.insert(std::make_pair(&argument, result));
	return result;
}

StoredValue Evaluator::visitConstantExpr(llvm::ConstantExpr & constantExpr) {
	llvm::Instruction * instruction = constantExpr.getAsInstruction();
	m_instsToDestroy.push_back(instruction);
	return visit(instruction);
}

StoredValue Evaluator::visitCastInst(llvm::CastInst & ci) {
	llvm::Value * operand = ci.getOperand(0);
	bool isConstParams = llvm::isa<llvm::Constant>(operand);
	StoredValueType type;
	if (ci.getType()->isPointerTy()) {
		if (operand->getType()->isPointerTy()) {
			StoredValue operandSV = visit(operand);
			type = operandSV.type;
		} else {
			type = StoredValueTypeUnknown;
		}
	} else {
		type = isConstParams ?
				StoredValueTypeConstant : StoredValueTypePrimitive;
	}
	StoredValue result(&ci, type);
	if (isConstParams) {
		m_cache.insert(std::make_pair(&ci, result));
	}
	return result;
}

StoredValue Evaluator::visitBinaryOperator(llvm::BinaryOperator & bo) {
	// As in cast:
	// 	If result is a pointer, then
	// 		if operands are a pointer and a constant, type is that of the pointer
	// 		else type is unknown
	// 	else
	// 		if both operands are constant, type is constant
	// 		else type is primitive
	// Cache only if both operands are constant
	llvm::Value * operand0 = bo.getOperand(0);
	llvm::Value * operand1 = bo.getOperand(1);
	bool isOp0Const = llvm::isa<llvm::Constant>(operand0);
	bool isOp1Const = llvm::isa<llvm::Constant>(operand1);
	bool isOp0Pointer = operand0->getType()->isPointerTy();
	bool isOp1Pointer = operand1->getType()->isPointerTy();
	StoredValueType type;
	if (bo.getType()->isPointerTy()) {
		if (isOp0Pointer && isOp1Const) {
			StoredValue op0SV = visit(operand0);
			type = op0SV.type;
		} else if (isOp1Pointer && isOp0Const) {
			StoredValue op1SV = visit(operand1);
			type = op1SV.type;
		} else {
			type = StoredValueTypeUnknown;
		}
	} else {
		type = (isOp0Const && isOp1Const) ?
				StoredValueTypeConstant : StoredValueTypePrimitive;
	}
	StoredValue result(&bo, type);
	if (isOp0Const && isOp1Const) {
		m_cache.insert(std::make_pair(&bo, result));
	}
	return result;
}

static const char * heap_allocation_functions[] = {
	"malloc",
	"realloc",
	0
};
StoredValue Evaluator::visitCallInst(llvm::CallInst & ci) {
	llvm::Function * function = ci.getCalledFunction();
	if (!function) {
		llvm::errs() << __PRETTY_FUNCTION__ << ": Return top\n";
		return StoredValue::top;
	}
	bool isHeapAlloc = false;
	const std::string & functionName = function->getName().str();
	for (int idx = 0; heap_allocation_functions[idx]; idx++) {
		if (functionName == heap_allocation_functions[idx]) {
			isHeapAlloc = true;
			break;
		}
	}
	if (!isHeapAlloc) {
		llvm::errs() << __PRETTY_FUNCTION__ << ": Return top\n";
		return StoredValue::top;
	}
	StoredValue result(&ci, StoredValueTypeHeap);
	return result;
}

MemoryAccessData::MemoryAccessData() : m_evaluator(stores, temporaries) {}
MemoryAccessData::~MemoryAccessData() {}

MemoryAccessInstVisitor::MemoryAccessInstVisitor() :
		llvm::InstVisitor<MemoryAccessInstVisitor>(), indirectFunctionCallCount(0) {}

MemoryAccessInstVisitor::~MemoryAccessInstVisitor() {
	for (std::map<const llvm::BasicBlock*, MemoryAccessData*>::iterator it = data.begin(),
										ie = data.end();
			it != ie; it++) {
		MemoryAccessData * data = it->second;
		delete data;
		it->second = 0;
	}
}

void MemoryAccessInstVisitor::visitFunction(llvm::Function & function) {
	this->function = &function;
}

void MemoryAccessInstVisitor::visitStoreInst(llvm::StoreInst & si) {
	llvm::Value * pointer = si.getPointerOperand();
	llvm::Value * value = si.getValueOperand();
	const llvm::BasicBlock * basicBlock = si.getParent();
	MemoryAccessData & data = getData(basicBlock);
	StoredValue storedPointer = data.m_evaluator.visit(pointer);
	StoredValue storedValue = data.m_evaluator.visit(value);
	const StoredValueType pointerType = storedPointer.type;
	// We want to keep working with the evaluated pointer.
	const llvm::Value * epointer = storedPointer.value;
	if (pointerType == StoredValueTypeStack) {
		StoredValues & values = data.stackStores[epointer];
		values.push_back(storedValue);
		joinStoredValues(data, epointer, values);
	} else if (pointerType == StoredValueTypeGlobal) {
		StoredValues & values = data.globalStores[epointer];
		values.push_back(storedValue);
		joinStoredValues(data, epointer, values);
	} else if (pointerType == StoredValueTypeArgument) {
		StoredValues & values = data.argumentStores[epointer];
		values.push_back(storedValue);
		joinStoredValues(data, epointer, values);
	} else if (pointerType == StoredValueTypeHeap) {
		StoredValues & values = data.heapStores[epointer];
		values.push_back(storedValue);
		joinStoredValues(data, epointer, values);
	} else {
		llvm::errs() << "This UNKNOWN is: " << storedPointer << "\n";
		StoredValues & values = data.unknownStores[epointer];
		values.push_back(storedValue);
		joinStoredValues(data, epointer, values);
	}
}

void MemoryAccessInstVisitor::joinStoredValues(MemoryAccessData & data, const llvm::Value * pointer, StoredValues & values) {
	if (values.size() == 1) {
		data.stores[pointer] = *values.begin();
	} else {
		data.stores[pointer] = StoredValue();
	}
}

void MemoryAccessInstVisitor::visitCallInst(llvm::CallInst & ci) {
	const llvm::Function * callee = ci.getCalledFunction();
	if (callee) {
		functionCalls.push_back(callee);
	} else {
		++indirectFunctionCallCount;
		llvm::errs() << "Indirect function call: " <<
				*(ci.getCalledValue()) << "\n";
	}
}

void MemoryAccessInstVisitor::insertNoDups(
		StoredValues &fromValues,
		StoredValues & toValues) const {
	StoredValues::iterator pos = toValues.begin();
	StoredValues inserted;
	for (StoredValues::iterator it = fromValues.begin(),
					ie = fromValues.end();
			it != ie; it++) {
		pos = std::lower_bound(pos, toValues.end(), *it);
		if (pos == toValues.end()) {
			inserted.insert(inserted.end(), it, ie);
			break;
		}
		if (*pos == *it) {
			continue;
		}
		inserted.push_back(*it);
		++pos;
	}
	toValues.insert(toValues.end(), inserted.begin(), inserted.end());
}

bool MemoryAccessInstVisitor::join(
		const StoreBaseToValuesMap & from,
		StoreBaseToValuesMap & to) const {
	bool result = false;
	for (StoreBaseToValuesMap::const_iterator it = from.begin(),
							ie = from.end();
			it != ie; it++) {
		StoreBaseToValuesMap::iterator found = to.find(it->first);
		if (found == to.end()) {
			to[it->first] = it->second;
		} else {
			// Copy fromValues
			StoredValues fromValues = it->second;
			StoredValues & toValues = found->second;
			std::sort(fromValues.begin(), fromValues.end());
			std::sort(toValues.begin(), toValues.end());
			result = result || !(std::includes(toValues.begin(), toValues.end(),
					fromValues.begin(), fromValues.end()));
			insertNoDups(fromValues, toValues);
		}
	}
	return result;
}

bool MemoryAccessInstVisitor::join(const std::map<const llvm::Value *, StoredValue> & from,
		std::map<const llvm::Value *, StoredValue> & to) const {
	bool result = false;
	for (std::map<const llvm::Value *, StoredValue>::const_iterator it = from.begin(),
									ie = from.end();
			it != ie; it++) {
		std::map<const llvm::Value *, StoredValue>::iterator found = to.find(it->first);
		if (found == to.end()) {
			to[it->first] = it->second;
			result = true;
		} else if (found->second != it->second) {
			// Replace with top
			found->second = StoredValue();
			result = true;
		}
	}
	return result;
}

bool MemoryAccessInstVisitor::join(const MemoryAccessData & from, MemoryAccessData & to) const {
	bool result = join(from.stackStores, to.stackStores) |
			join(from.globalStores, to.globalStores) |
			join(from.argumentStores, to.argumentStores) |
			join(from.unknownStores, to.unknownStores) |
			join(from.temporaries, to.temporaries) |
			join(from.stores, to.stores);
	return result;
}

bool MemoryAccessInstVisitor::join(const llvm::BasicBlock * from, const llvm::BasicBlock * to) {
	bool result = false;
	MemoryAccessData & fromData = getData(from);
	if (data.find(to) == data.end()) {
		result = true;
	}
	MemoryAccessData & toData = getData(to);
	return join(fromData, toData) || result;
}

MemoryAccessData & MemoryAccessInstVisitor::getData(const llvm::BasicBlock * bb) {
	// Optimisation: Use lower_bound as hint
	std::map<const llvm::BasicBlock*, MemoryAccessData*>::iterator it =
			data.find(bb);
	if (it != data.end()) {
		return *(it->second);
	}
	MemoryAccessData * presult = new MemoryAccessData();
	data[bb] = presult;
	return *presult;
}

}
