#include <algorithm>

#include <MemoryAccessInstVisitor.h>
#include <llvm/Support/raw_ostream.h>

namespace MemoryAccessPass {

StoredValue* Evaluator::visitGlobalValue(llvm::GlobalValue & globalValue) {
	llvm::errs() << "Enter " << __PRETTY_FUNCTION__ << "\n";
	const llvm::Value * value = &globalValue;
	StoredValue* result = new StoredValue(value, StoredValueTypeGlobal);
	m_cache.insert(std::make_pair(value, result));
	return result;
}

StoredValue* Evaluator::visitAllocaInst(llvm::AllocaInst & allocaInst) {
	llvm::errs() << "Enter " << __PRETTY_FUNCTION__ << "\n";
	const llvm::Value * value = &allocaInst;
	StoredValue* result = new StoredValue(value, StoredValueTypeStack);
	m_cache.insert(std::make_pair(value, result));
	return result;
}

MemoryAccessData::MemoryAccessData() : m_evaluator(temporaries) {}
MemoryAccessData::~MemoryAccessData() {}

MemoryAccessInstVisitor::MemoryAccessInstVisitor() :
		llvm::InstVisitor<MemoryAccessInstVisitor>(), indirectFunctionCallCount(0) {}

MemoryAccessInstVisitor::~MemoryAccessInstVisitor() {}

void MemoryAccessInstVisitor::visitFunction(llvm::Function & function) {
	this->function = &function;
}

void MemoryAccessInstVisitor::visitStoreInst(llvm::StoreInst & si) {
	llvm::errs() << "Store instruction: " << si << "\n";
	const llvm::Value * pointer = si.getPointerOperand();
	llvm::Value * value = si.getValueOperand();
	const llvm::BasicBlock * basicBlock = si.getParent();
	MemoryAccessData & data = getData(basicBlock);
	StoredValue* storedValue = data.m_evaluator.visit(value);
	if (!storedValue) {
		llvm::errs() << "storedValue is null for: " << *value << "\n";
		return;
	}
	const StoredValueType valueType = storedValue->type;
	if (valueType == StoredValueTypeStack) {
		StoredValues & values = data.stackStores[pointer];
		values.push_back(storedValue);
	} else if (valueType == StoredValueTypeGlobal) {
		StoredValues & values = data.globalStores[pointer];
		values.push_back(storedValue);
	} else {
		StoredValues & values = data.unknownStores[pointer];
		values.push_back(storedValue);
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

bool MemoryAccessInstVisitor::join(const MemoryAccessData & from, MemoryAccessData & to) const {
	bool result = join(from.stackStores, to.stackStores) |
			join(from.globalStores, to.globalStores) |
			join(from.unknownStores, to.unknownStores);
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
