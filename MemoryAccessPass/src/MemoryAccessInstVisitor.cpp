#include <algorithm>

#include <MemoryAccessInstVisitor.h>
#include <llvm/Support/raw_ostream.h>

namespace MemoryAccessPass {

MemoryAccessInstVisitor::MemoryAccessInstVisitor() :
		llvm::InstVisitor<MemoryAccessInstVisitor>(), indirectFunctionCallCount(0) {}

void MemoryAccessInstVisitor::visitFunction(llvm::Function & function) {
	this->function = &function;
}

void MemoryAccessInstVisitor::visitStoreInst(llvm::StoreInst & si) {
	const llvm::Value * pointer = si.getPointerOperand();
	const llvm::Value * value = si.getValueOperand();
	const llvm::BasicBlock * basicBlock = si.getParent();
	MemoryAccessData & data = this->data[basicBlock];
	llvm::errs() << "Store instruction: " << si << "\n";
	if (llvm::isa<llvm::AllocaInst>(pointer)) {
		MemoryAccessData::StoredValues & values = data.stackStores[pointer];
		values.push_back(value);
	} else if (llvm::isa<llvm::GlobalValue>(pointer)) {
		MemoryAccessData::StoredValues & values = data.globalStores[pointer];
		values.push_back(value);
	} else {
		MemoryAccessData::StoredValues & values = data.unknownStores[pointer];
		values.push_back(value);
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
		MemoryAccessData::StoredValues &fromValues,
		MemoryAccessData::StoredValues & toValues) const {
	MemoryAccessData::StoredValues::iterator pos = toValues.begin();
	MemoryAccessData::StoredValues inserted;
	for (MemoryAccessData::StoredValues::iterator it = fromValues.begin(),
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
		MemoryAccessData::StoreBaseToValuesMap & from,
		MemoryAccessData::StoreBaseToValuesMap & to) const {
	bool result = false;
	for (MemoryAccessData::StoreBaseToValuesMap::iterator it = from.begin(),
								ie = from.end();
			it != ie; it++) {
		MemoryAccessData::StoreBaseToValuesMap::iterator found =
				to.find(it->first);
		if (found == to.end()) {
			to[it->first] = it->second;
		} else {
			MemoryAccessData::StoredValues & fromValues = it->second;
			MemoryAccessData::StoredValues & toValues = found->second;
			std::sort(fromValues.begin(), fromValues.end());
			std::sort(toValues.begin(), toValues.end());
			result = result || !(std::includes(toValues.begin(), toValues.end(),
					fromValues.begin(), fromValues.end()));
			insertNoDups(fromValues, toValues);
		}
	}
	return result;
}

bool MemoryAccessInstVisitor::join(MemoryAccessData & from, MemoryAccessData & to) const {
	bool result = join(from.stackStores, to.stackStores) |
			join(from.globalStores, to.globalStores) |
			join(from.unknownStores, to.unknownStores);
	return result;
}

bool MemoryAccessInstVisitor::join(const llvm::BasicBlock * from, const llvm::BasicBlock * to) {
	bool result = false;
	MemoryAccessData & fromData = data[from];
	if (data.find(to) == data.end()) {
		result = true;
	}
	MemoryAccessData & toData = data[to];
	return join(fromData, toData) || result;
}

}
