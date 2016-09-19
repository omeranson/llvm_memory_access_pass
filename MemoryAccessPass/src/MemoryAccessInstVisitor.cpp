#include <algorithm>

#include <MemoryAccessInstVisitor.h>
#include <llvm/Support/raw_ostream.h>

namespace MemoryAccessPass {

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
	llvm::errs() << "Enter " << __PRETTY_FUNCTION__ << "\n";
	llvm::Value * pointer = loadInst.getPointerOperand();
	StoredValue result = m_stores[pointer];
	return result;
}

StoredValue Evaluator::visitGetElementPtrInst(llvm::GetElementPtrInst & gepInst) {
	llvm::Value * pointer = gepInst.getPointerOperand();
	StoredValue pointerStoredValue = visit(pointer);
	StoredValueType pointerStoredValueType = pointerStoredValue.type;
	for (llvm::User::op_iterator it = gepInst.idx_begin(),
					ie = gepInst.idx_end();
			it != ie; it++) {
		llvm::Value * operand = *it;
		StoredValue operandStoredValue = visit(operand);
		if (operandStoredValue.type != StoredValueTypeConstant) {
			pointerStoredValueType = StoredValueTypeUnknown;
		}
	}
	StoredValue result(pointer, pointerStoredValueType);
	return result;
}

StoredValue Evaluator::visitArgument(llvm::Argument & argument) {
	StoredValueType type = argument.getType()->isPointerTy() ?
			StoredValueTypeUnknown : StoredValueTypePrimitive;
	StoredValue result(&argument, type);
	m_cache.insert(std::make_pair(&argument, result));
	return result;
}

StoredValue Evaluator::visitConstantExpr(llvm::ConstantExpr & constantExpr) {
	llvm::Instruction * instruction = constantExpr.getAsInstruction();
	m_instsToDestroy.push_back(instruction);
	return visit(instruction);
}


MemoryAccessData::MemoryAccessData() : m_evaluator(temporaries) {}
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
	llvm::errs() << "Store instruction: " << si << "\n";
	llvm::Value * pointer = si.getPointerOperand();
	llvm::Value * value = si.getValueOperand();
	const llvm::BasicBlock * basicBlock = si.getParent();
	MemoryAccessData & data = getData(basicBlock);
	StoredValue storedPointer = data.m_evaluator.visit(pointer);
	StoredValue storedValue = data.m_evaluator.visit(value);
	const StoredValueType pointerType = storedPointer.type;
	if (pointerType == StoredValueTypeStack) {
		StoredValues & values = data.stackStores[pointer];
		values.push_back(storedValue);
		joinStoredValues(data, pointer, values);
	} else if (pointerType == StoredValueTypeGlobal) {
		StoredValues & values = data.globalStores[pointer];
		values.push_back(storedValue);
		joinStoredValues(data, pointer, values);
	} else {
		llvm::errs() << "This UNKNOWN is: " << storedPointer.value << "\n";
		StoredValues & values = data.unknownStores[pointer];
		values.push_back(storedValue);
		joinStoredValues(data, pointer, values);
	}
}

void MemoryAccessInstVisitor::joinStoredValues(MemoryAccessData & data, llvm::Value * pointer, StoredValues & values) {
	if (values.size() == 1) {
		data.stores[pointer] = StoredValue(*values.begin());
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
		} else if (found->second == it->second) {
			// Do nothing
		} else {
			// Replace with top
			found->second = StoredValue();
		}
	}
	return result;
}

bool MemoryAccessInstVisitor::join(const MemoryAccessData & from, MemoryAccessData & to) const {
	bool result = join(from.stackStores, to.stackStores) |
			join(from.globalStores, to.globalStores) |
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
