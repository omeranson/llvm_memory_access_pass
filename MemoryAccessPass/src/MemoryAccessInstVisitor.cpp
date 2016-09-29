#include <algorithm>
#include <cassert>

#include <llvm/Support/raw_ostream.h>

#include <ChaoticIteration.h>
#include <MemoryAccessInstVisitor.h>

namespace MemoryAccessPass {

bool isPredefinedFunction(llvm::Function & F);

int MemoryAccessArgumentAccessWatermark = 10;
int MemoryAccessGlobalAccessWatermark = 10;
int MemoryAccessFunctionCallCountWatermark = 10;
int VisitBlockCountWatermark = 10;

StoredValue StoredValue::top = StoredValue();

StoredValue Evaluator::visitGlobalValue(llvm::GlobalValue & globalValue) {
	llvm::Value * value = &globalValue;
	StoredValue result(value, StoredValueTypeGlobal);
	m_cache.insert(std::make_pair(value, result));
	return result;
}

StoredValue Evaluator::visitAllocaInst(llvm::AllocaInst & allocaInst) {
	llvm::Value * value = &allocaInst;
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
		llvm::InstVisitor<MemoryAccessInstVisitor>(),
		visitBlockCount(0), haveIHadEnough(false),
		function(0), functionData(0) {}

MemoryAccessInstVisitor::~MemoryAccessInstVisitor() {
	delete functionData;
	for (std::map<const llvm::BasicBlock*, MemoryAccessData*>::iterator it = data.begin(),
										ie = data.end();
			it != ie; it++) {
		MemoryAccessData * data = it->second;
		delete data;
		it->second = 0;
	}
}

void MemoryAccessInstVisitor::runOnFunction(llvm::Function & F, MemoryAccessCache * cache) {
	if (isPredefinedFunction(F)) {
		functionData = new MemoryAccessData();
		return;
	}
	ChaoticIteration<MemoryAccessInstVisitor> chaoticIteration(*this);
	chaoticIteration.iterate(F);
	join(cache);
}

bool MemoryAccessInstVisitor::isSummariseFunction() const {
	if (haveIHadEnough) {
		return false;
	}
	if (functionData->indirectFunctionCalls.size() > 0) {
		return false;
	}
	if (functionData->unknownStores.size() > 0) {
		return false;
	}
	if (functionData->heapStores.size() > 0) {
		return false;
	}
	if (functionData->argumentStores.size() > MemoryAccessArgumentAccessWatermark) {
		return false;
	}
	if (functionData->globalStores.size() > MemoryAccessGlobalAccessWatermark) {
		return false;
	}
	if (functionData->functionCalls.size() > MemoryAccessFunctionCallCountWatermark) {
		return false;
	}
	return true;
}

void MemoryAccessInstVisitor::visitFunction(llvm::Function & function) {
	assert((!this->function) && "MemoryAccessInstVisitor::visitFunction called more than once");
	this->function = &function;
}

void MemoryAccessInstVisitor::visitBasicBlock(llvm::BasicBlock & basicBlock) {
	if (++visitBlockCount > VisitBlockCountWatermark) {
		haveIHadEnough = true;
	}
}

void MemoryAccessInstVisitor::visitStoreInst(llvm::StoreInst & si) {
	llvm::Value * pointer = si.getPointerOperand();
	llvm::Value * value = si.getValueOperand();
	const llvm::BasicBlock * basicBlock = si.getParent();
	MemoryAccessData & data = getData(basicBlock);
	StoredValue storedPointer = data.m_evaluator.visit(pointer);
	StoredValue storedValue = data.m_evaluator.visit(value);
	store(data, storedPointer, storedValue);
}

void MemoryAccessInstVisitor::store(MemoryAccessData & data,
		StoredValue & pointer, StoredValue & value) {
	if (haveIHadEnough) {
		return;
	}
	const llvm::Value * epointer = pointer.value;
	StoredValue joinedValue = joinStoredValues(data.stores, epointer, value);
	StoreBaseToValueMap * specialisedStores;
	const StoredValueType pointerType = pointer.type;
	if (pointerType == StoredValueTypeStack) {
		specialisedStores = &(data.stackStores);
	} else if (pointerType == StoredValueTypeGlobal) {
		specialisedStores = &(data.globalStores);
	} else if (pointerType == StoredValueTypeArgument) {
		specialisedStores = &(data.argumentStores);
	} else if (pointerType == StoredValueTypeHeap) {
		specialisedStores = &(data.heapStores);
	} else {
		llvm::errs() << "This UNKNOWN is: " << pointer << "\n";
		specialisedStores = &(data.unknownStores);
	}
	specialisedStores->insert(std::make_pair(epointer, joinedValue));
}

StoredValue MemoryAccessInstVisitor::joinStoredValues(
		StoreBaseToValueMap & stores,
		const llvm::Value * epointer, const StoredValue &value) const {
	StoreBaseToValueMap::iterator it = stores.find(epointer);
	if (it == stores.end()) {
		stores.insert(std::make_pair(epointer, value));
		return value;
	} else if (it->second != value) {
		// Constant propogation - join -> top
		it->second = StoredValue::top;
		return StoredValue::top;
	} else {
		return value;
	}
}

void MemoryAccessInstVisitor::visitCallInst(llvm::CallInst & ci) {
	if (llvm::isa<llvm::DbgInfoIntrinsic>(&ci)) {
		return;
	}
	const llvm::BasicBlock * basicBlock = ci.getParent();
	MemoryAccessData & data = getData(basicBlock);
	const llvm::Function * callee = ci.getCalledFunction();
	if (callee) {
		data.functionCalls.insert(&ci);
	} else {
		data.indirectFunctionCalls.insert(&ci);
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
		const StoreBaseToValueMap & from,
		StoreBaseToValueMap & to) const {
	bool result = false;
	for (StoreBaseToValueMap::const_iterator it = from.begin(),
							ie = from.end();
			it != ie; it++) {
		if (joinStoredValues(to, it->first, it->second) != it->second) {
			result = true;
		}
	}
	return result;
}

template <class T>
bool MemoryAccessInstVisitor::join(const std::set<const T *> & from,
		std::set<const T *> & to) const {
	bool result = !(std::includes(to.begin(), to.end(),
			from.begin(), from.end()));
	to.insert(from.begin(), from.end());
	return result;
}

bool MemoryAccessInstVisitor::join(const MemoryAccessData & from, MemoryAccessData & to) const {
	bool result = join(from.stackStores, to.stackStores) |
			join(from.globalStores, to.globalStores) |
			join(from.argumentStores, to.argumentStores) |
			join(from.heapStores, to.heapStores) |
			join(from.unknownStores, to.unknownStores) |
			join(from.temporaries, to.temporaries) |
			join(from.stores, to.stores) |
			join(from.functionCalls, to.functionCalls) |
			join(from.indirectFunctionCalls, to.indirectFunctionCalls);
	return result;
}

bool MemoryAccessInstVisitor::join(const llvm::BasicBlock * from, const llvm::BasicBlock * to) {
	if (haveIHadEnough) {
		return false;
	}
	bool result = false;
	MemoryAccessData & fromData = getData(from);
	if (data.find(to) == data.end()) {
		result = true;
	}
	MemoryAccessData & toData = getData(to);
	return join(fromData, toData) || result;
}

void MemoryAccessInstVisitor::join(MemoryAccessCache * cache) {
	assert((!functionData) && "MemoryAccessInstVisitor::join called more than once");
	functionData = new MemoryAccessData();
	for (llvm::Function::iterator it = function->begin(),
				ie = function->end();
			it != ie; it++) {
		const MemoryAccessData &bb_data = getData(it);
		join(bb_data, *functionData);
	}
	if (!cache) {
		return;
	}
	// Now join over function calls
	// TODO(oanson) Handle recursive calls
	for (std::set<const llvm::CallInst *>::iterator it = functionData->functionCalls.begin(),
						ie = functionData->functionCalls.end();
			it != ie; it++) {
		const llvm::CallInst * ci = *it;
		joinCall(*ci, cache);
	}
}

bool MemoryAccessInstVisitor::joinCall(const llvm::CallInst & ci, MemoryAccessCache * cache) {
	llvm::Function * F = ci.getCalledFunction();
	if (isPredefinedFunction(*F)) {
		return false;
	}
	llvm::errs() << "Joining called function: " << F->getName() << "\n";
	const MemoryAccessInstVisitor * visitor = cache->getVisitor(F);
	// Cache already returns visitor after call to join, so no need
	// for nested treatment
	const MemoryAccessData & calleeData = *(visitor->functionData);
	MemoryAccessData & data = *functionData;
	bool result = false;
	result |= join(calleeData.globalStores, data.globalStores);
	result |= join(calleeData.heapStores, data.unknownStores);
	result |= join(calleeData.unknownStores, data.unknownStores);
	result |= joinCalleeArguments(ci, visitor);
	if ((!haveIHadEnough) && (visitor->haveIHadEnough)) {
		haveIHadEnough = true;
		result = true;
	}
	return result;
}

bool MemoryAccessInstVisitor::joinCalleeArguments(const llvm::CallInst & ci,
		const MemoryAccessInstVisitor * visitor) {
	const MemoryAccessData & calleeData = *(visitor->functionData);
	MemoryAccessData & data = *functionData;
	// TODO(oanson) result value may be calculated wrongly.
	bool result = false;
	for (StoreBaseToValueMap::const_iterator it = calleeData.argumentStores.begin(),
						ie = calleeData.argumentStores.end();
			it != ie; it++) {
		const llvm::Value * argumentValue = it->first;
		const llvm::Argument * argument = llvm::dyn_cast<llvm::Argument>(argumentValue);
		if (!argument) {
			llvm::errs() << "Store to inner argument, but not an argument: " << *argumentValue << "\n";
			data.unknownStores.insert(std::make_pair(it->first, it->second));
			result = true;
			continue;
		}
		unsigned index = argument->getArgNo();
		llvm::Value * parameter = ci.getArgOperand(index);
		StoredValue value = data.m_evaluator.visit(parameter);
		if (value.isTop()) {
			llvm::errs() << "Store to inner argument, but operand is top: " << *parameter << "\n";
			data.unknownStores.insert(std::make_pair(parameter, it->second));
			result = true;
			continue;
		}
		llvm::Value * evaluatedParameter = value.value;
		StoredValue storedEvaluatedParameter = data.m_evaluator.visit(evaluatedParameter);
		store(data, storedEvaluatedParameter, value);
		result = true;
	}
	return result;
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
