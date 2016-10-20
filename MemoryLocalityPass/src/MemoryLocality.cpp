#include <llvm/Analysis/CallGraph.h>
#include <llvm/Analysis/MemoryDependenceAnalysis.h>
#include <llvm/Analysis/PHITransAddr.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>

#include <dsa/AllocatorIdentification.h>

//#include <MemoryDependenceAnalysis.h>
#include <MemoryLocality.h>
#include <ValueVisitor.h>

#define PHI_DEPTH_WATERMARK 10

namespace llvm {
class FunctionInstructionIterator {
protected:
	Function * F;
	Function::iterator BBIter;
	BasicBlock * BB;
	BasicBlock::iterator InstIter;
	Instruction * Inst;
public:
	FunctionInstructionIterator(Function & F) : F(&F), BBIter(F.end()), BB(0), Inst(0) {}
	FunctionInstructionIterator(Function * F) : F(F), BBIter(F->end()), BB(0), Inst(0) {}
	~FunctionInstructionIterator() {}
	FunctionInstructionIterator begin() const {
		FunctionInstructionIterator result(F);
		result.BBIter = F->begin();
		if (result.BBIter == F->end()) {
			return result;
		}
		result.BB = &(*result.BBIter);
		result.InstIter = result.BB->begin();
		if (result.InstIter != result.BB->end()) {
			result.Inst = &(*result.InstIter);
		}
		return result;
	}
	FunctionInstructionIterator end() const {
		FunctionInstructionIterator result(F);
		return result;
	}

	Instruction * operator*() {
		assert((*this != end()) && "Attempting to dereference iterator at end");
		return Inst;
	}

	FunctionInstructionIterator & operator++() {
		assert((*this != end()) && "Attempting to increment iterator at end");
		InstIter++;
		while (InstIter == BB->end()) {
			if (++BBIter == F->end()) {
				BB = 0;
				Inst = 0;
				return *this;
			}
			BB = &(*BBIter);
			InstIter = BB->begin();
		}
		Inst = &(*InstIter);
		return *this;
	}

	FunctionInstructionIterator operator++(int) {
		assert((*this != end()) && "Attempting to increment iterator at end");
		FunctionInstructionIterator result(*this);
		++(*this);
		return result;
	}

	//operator--
	//--operator

	bool operator==(const FunctionInstructionIterator & other) const {
		if (F != other.F) {
			return false;
		}
		if (BBIter != other.BBIter) {
			return false;
		}
		if (BBIter == F->end()) {
			// The rest of the members are undefined in this case
			return true;
		}
		if (BB != other.BB) {
			// Should never happend. Will fail on prev. case.
			return false;
		}
		if (InstIter != other.InstIter) {
			return false;
		}
		if (Inst != other.Inst) {
			// Should never happend. Will fail on prev. case.
			return false;
		}
		return true;
	}

	bool operator!=(const FunctionInstructionIterator & other) const {
		return !(other == *this);
	}

	bool atEnd() const {
		return (BBIter == F->end());
	}
};

}
namespace MemoryLocality {

class MemoryDependenceAnalysis : public llvm::MemoryDependenceAnalysis {
public:
	static char ID;
	MemoryDependenceAnalysis() : llvm::MemoryDependenceAnalysis() {}
	virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const {
		const llvm::PassInfo * PI = lookupPassInfo(llvm::StringRef("basicaa"));
		AU.addRequiredID(PI->getTypeInfo());
		llvm::MemoryDependenceAnalysis::getAnalysisUsage(AU);
	}
};

char MemoryDependenceAnalysis::ID = 0;
static llvm::RegisterPass<MemoryDependenceAnalysis> _W(
		"memdep2",
		"MemoryDependenceAnalysis with forced basicaa",
		false, false);

struct PointerSourceEvaluator : public llvm::ValueVisitor<PointerSourceEvaluator> {
	PointerSource pointerSource;
	std::vector<PointerSource> & arguments;
	MemoryDependenceAnalysis * mda;
	llvm::AliasAnalysis * AA;
	llvm::DataLayout * DL;
	llvm::AllocIdentify * AI;
	int phidepth;

	PointerSourceEvaluator(std::vector<PointerSource> & arguments, MemoryDependenceAnalysis * mda, llvm::AliasAnalysis * AA, llvm::DataLayout * DL, llvm::AllocIdentify * AI) :
			llvm::ValueVisitor<PointerSourceEvaluator>(), arguments(arguments), mda(mda), AA(AA), DL(DL), AI(AI), phidepth(0) {}
	~PointerSourceEvaluator() {}
	void clear() {
		pointerSource.clear();
		arguments.clear();
	}
	void visitArgument(llvm::Argument &A) {
		if (arguments.empty()) {
			pointerSource.type = PointerSource_Argument;
			pointerSource.argument = &A;
			return;
		}
		pointerSource = arguments[A.getArgNo()];
		if (pointerSource.type == PointerSource_Local) {
			pointerSource.type = PointerSource_Function;
		}
	}
	void visitGlobalValue(llvm::GlobalValue &GV) {
		pointerSource.type = PointerSource_Global;
		pointerSource.name = GV.getName();
	}
	void visitAllocaInst(llvm::AllocaInst &AI) {
		pointerSource.type = PointerSource_Local;
		pointerSource.name = AI.getParent()->getParent()->getName();
	}
	void visitGetElementPtrInst(llvm::GetElementPtrInst &GEPI) {
		llvm::Value * pointer = GEPI.getPointerOperand();
		visit(pointer);
	}

	void visitCastInst(llvm::CastInst & ci) {
		llvm::Value * operand = ci.getOperand(0);
		visit(operand);
	}

	void visitCallInst(llvm::CallInst &CI) {
		llvm::Function * calledFunction = CI.getCalledFunction();
		//assert(calledFunction && "Indirect function calls are not yet supported");
		if (calledFunction) {
			pointerSource.type = PointerSource_Function;
			if (AI->isAllocator(calledFunction->getName())) {
				pointerSource.name = CI.getParent()->getParent()->getName();
			} else {
				pointerSource.name = calledFunction->getName();
			}
		} else {
			pointerSource.type = PointerSource_Unknown;
		}
	}

	void printMDR(const llvm::Value * value, const llvm::MemDepResult & mdr) {
		llvm::errs() << "\tMemdep result for " << *value << ":\n";
		llvm::errs() << "\t\tisClobber: " << mdr.isClobber();
		if (mdr.isClobber()) {
			llvm::errs() << ": " << *mdr.getInst();
		}
		llvm::errs() << "\n";
		llvm::errs() << "\t\tisDef: " << mdr.isDef();
		if (mdr.isDef()) {
			llvm::errs() << ": " << *mdr.getInst();
		}
		llvm::errs() << "\n";
		llvm::errs() << "\t\tisNonLocal: " << mdr.isNonLocal() << "\n";
		llvm::errs() << "\t\tisNonFuncLocal: " << mdr.isNonFuncLocal() << "\n";
		llvm::errs() << "\t\tisUnknown: " << mdr.isUnknown() << "\n";
	}

	bool evaluateLoadNonLocal(llvm::LoadInst & LI, llvm::BasicBlock * BB) {
		const llvm::AliasAnalysis::Location &Loc = AA->getLocation(&LI);
		llvm::MemDepResult mdr = mda->getPointerDependencyFrom(Loc, true, BB->end(), BB, &LI);
		if (mdr.isDef()) {
			if (visitDefMDR(LI, mdr)) {
				return true;
			}
		}
		return false;
	}

	bool visitDefMDR(llvm::LoadInst & LI, const llvm::MemDepResult & mdr) {
		llvm::Instruction * inst = mdr.getInst();
		if (llvm::StoreInst * si = llvm::dyn_cast<llvm::StoreInst>(inst)) {
			llvm::Value * value = si->getValueOperand();
			visit(value);
			return true;
		}
		if (llvm::LoadInst * li = llvm::dyn_cast<llvm::LoadInst>(inst)) {
			// A must alias?
			if (li == &LI) {
				// Recursion?
				return false;
			}
			visit(li);
			return true;
		}
		return false;
	}

	void visitLoadInst(llvm::LoadInst &LI) {
		if (!mda) {
			llvm::errs() << "No mda for this function\n";
			return;
		}
		const llvm::MemDepResult mdr = mda->getDependency(&LI);
		if (mdr.isDef()) {
			if (visitDefMDR(LI, mdr)) {
				return;
			}
		}
		// AA currently doesn't handle GEPIs well. So we cheat.
		llvm::Value * pointer = LI.getPointerOperand();
		pointerSource.clear();
		visit(pointer);
		if (pointerSource.type != PointerSource_Unknown) {
			return;
		}
		if (mdr.isNonLocal()) {
			llvm::SmallVector<llvm::NonLocalDepResult, 32> Result;
			llvm::DenseMap<llvm::BasicBlock*, llvm::Value*> Visited;
			const llvm::AliasAnalysis::Location &Loc = AA->getLocation(&LI);
			mda->getNonLocalPointerDependency(
					Loc, true, LI.getParent(), Result);
			for (unsigned idx = 0; idx < Result.size(); idx++) {
				llvm::NonLocalDepResult & result = Result[idx];
				if (result.getResult().isDef()) {
					if (visitDefMDR(LI, result.getResult())) {
						return;
					}
				}
				if (evaluateLoadNonLocal(LI, result.getBB())) {
					// TODO Join accross all non-local results?
					return;
				}
			}
		}
		llvm::errs() << "Failed to evaluate load: " << LI << " in " << LI.getParent()->getParent()->getName() << "\n";
		printMDR(&LI, mdr);
	}

	void visitPHINode(llvm::PHINode &I) {
		if (phidepth >= PHI_DEPTH_WATERMARK) {
			return;
		}
		++phidepth;
		pointerSource.clear();
		for (unsigned idx = 0; idx < I.getNumIncomingValues(); idx++) {
			llvm::Value * value = I.getIncomingValue(idx);
			visit(I.getIncomingValue(idx));
			// TODO Join accross all phis?
			if (pointerSource.type != PointerSource_Unknown) {
				return;
			}
		}
		llvm::errs() << "Phi node: " << I << " could not be evaluated\n";
		--phidepth;
	}

	void visitInstruction(llvm::Instruction & inst) {
		llvm::errs() << "Unhandled instruction " << inst << " opcode " << inst.getOpcode() << " " << inst.getOpcodeName() << "\n";
	}

	void visitConstantExpr(llvm::ConstantExpr & constantExpr) {
		llvm::Instruction * inst = constantExpr.getAsInstruction();
		visit(inst);
		delete inst;
	}
	void visitConstant(llvm::Constant & constant) {
		llvm::errs() << "Unhandled constant " << constant << "\n";
	}
	void visitConstantPointerNull(llvm::ConstantPointerNull & constant) {
		pointerSource.type = PointerSource_Global;
		pointerSource.name = "null";
	}
	void visitUndefValue(llvm::UndefValue & undefValue) {
		// Do nothing.
		// By existing, this method prevents the warning in visitConstant.
	}
};

struct LocalityFunctionVisitor : public llvm::InstVisitor<LocalityFunctionVisitor> {
	PointerSourceEvaluator visitor;
	PointerSource source;
	WorkQueueItem workItem;
	WorkQueueItem newWorkItem;
	std::set<std::string> outgoingEdges;
	llvm::FunctionInstructionIterator iterator;
	llvm::Instruction * instruction;
	bool isModified;
	bool isCall;
	bool isFinished;

	LocalityFunctionVisitor(
			WorkQueueItem & item,
			MemoryDependenceAnalysis * mda, llvm::AliasAnalysis * AA, llvm::DataLayout * DL, llvm::AllocIdentify * AI) : 
					visitor(item.argumentSources, mda, AA, DL, AI),
					workItem(item),
					iterator(*item.function) {}

	PointerSource & evaluate(llvm::Value * value) {
		// TODO Add caching
		visitor.pointerSource.clear();
		if (value->getType()->isPointerTy()) {
			visitor.visit(value);
		} else {
			visitor.pointerSource.type = PointerSource_Primitive;
		}
		return visitor.pointerSource;
	}

	void evaluateAndStorePointerSource(llvm::Value * pointer) {
		source = evaluate(pointer);
		isModified = true;
	}

	void addEdge(const std::string & v) {
		outgoingEdges.insert(v);
	}

	void visitLoadInst(llvm::LoadInst &LI) {
		llvm::Value * pointer = LI.getPointerOperand();
		evaluateAndStorePointerSource(pointer);
	}

	void visitStoreInst(llvm::StoreInst &SI) {
		llvm::Value * pointer = SI.getPointerOperand();
		evaluateAndStorePointerSource(pointer);
	}

	void visitCallInst(llvm::CallInst &CI) {
		llvm::Function * calledFunction = CI.getCalledFunction();
		//assert(calledFunction && "Indirect function calls are not yet supported");
		if (!calledFunction) {
			addEdge("Unknown locality (INACCURACY, Indirect function call)");
			return;
		}
		// Found this function. Add it to the work queue, with all its
		// arguments
		// 1. Populate arguments
		newWorkItem.clear();
		newWorkItem.function = calledFunction;
		newWorkItem.callers = workItem.callers;
		if (!newWorkItem.callers.insert(calledFunction).second) {
			addEdge("Unknown locality (INACCURACY, Recursion)");
			return;
		}
		for (unsigned idx = 0; idx < CI.getNumArgOperands(); idx++) {
			llvm::Value * value = CI.getArgOperand(idx);
			PointerSource & pointerSource = evaluate(value);
			//llvm::errs() << "Argument " << calledFunction->getName()
			//		<< "#" << newWorkItem.argumentSources.size()
			//		<< " is " << pointerSource.type << " "
			//		<< pointerSource.name << "\n";
			newWorkItem.argumentSources.push_back(pointerSource);
		}
		// 2. Add to work queue
		isCall = true;
	}

	void start() {
		iterator = iterator.begin();
		isFinished = iterator.atEnd();
	}

	void visitNext() {
		isModified = false;
		isCall = false;
		instruction = *iterator;
		visit(instruction);
		isFinished = (++iterator).atEnd();
	}
};

void MemoryLocality::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
	AU.addRequired<llvm::AliasAnalysis>();
	AU.addRequired<MemoryDependenceAnalysis>();
	AU.addRequired<llvm::CallGraph>();
	AU.addRequired<llvm::DataLayout>();
	AU.addRequired<llvm::AllocIdentify>();
	AU.setPreservesAll();
}

void __attribute__((noinline)) printModRef(llvm::AliasAnalysis & AA, llvm::Function *F) {
	const llvm::AliasAnalysis::ModRefBehavior b = AA.getModRefBehavior(F);
	llvm::errs() << "Mod ref behaviour for this function: " << b << "\n";
}

void __attribute__((noinline)) printAAs(llvm::AnalysisResolver * AR, const llvm::PassInfo * PI) {
	llvm::Pass * p = AR->getAnalysisIfAvailable(PI->getTypeInfo(), true);
	llvm::errs() << "basicaa pass: " << (p ? p->getPassName() : "(not found)") << "\n";
	llvm::Pass * AAP = AR->getAnalysisIfAvailable(&llvm::AliasAnalysis::ID, true);
	llvm::errs() << "AA pass: " << (AAP ? AAP->getPassName() : "(not found)") << "\n";
}

bool MemoryLocality::runOnModule(llvm::Module &M) {
	//const llvm::PassInfo * PI = lookupPassInfo(llvm::StringRef("basicaa"));
	//printAAs(getResolver(), PI);

	WorkQueueItem rootItem;
	rootItem.function = getRoot(M);
	rootItem.argumentSources.push_back(PointerSource("argc", PointerSource_Global, 0));
	rootItem.argumentSources.push_back(PointerSource("argv", PointerSource_Global, 0));
	rootItem.argumentSources.push_back(PointerSource("envp", PointerSource_Global, 0));
	assert(rootItem.function && "Could not find root function");
	rootItem.callers.insert(rootItem.function);
	callAdded(rootItem);
	while (!localityVisitorsStack.empty()) {
		visit();
	}
	return false;
}

void MemoryLocality::visit() {
	LocalityFunctionVisitor * visitor = localityVisitorsStack.back();
	visitor->visitNext();
	if (visitor->isCall) {
		callAdded(visitor->newWorkItem);
	}
	if (visitor->isModified) {
		PointerSource & source = visitor->source;
		switch (source.type) {
			case PointerSource_Primitive:
			case PointerSource_Local:
				// Do nothing;
				break;
			case PointerSource_Global:
				addEdge(visitor->workItem.function->getName(), "Global objects");
				break;
			case PointerSource_Argument:
				addEdge(visitor->workItem.function->getName(), "Unevaluated argument (ERROR)");
				break;
			case PointerSource_Function:
				addEdge(visitor->workItem.function->getName(), source.name);
				break;
			case PointerSource_Unknown:
				addEdge(visitor->workItem.function->getName(), "Unknown locality (INACCURACY, Pointer Evaluation)");
				llvm::errs() << "Couldn't evaluate source for " << *(visitor->instruction) << " in " << visitor->workItem.function->getName() << "\n";
				break;
		}
	}
	if (visitor->isFinished) {
		localityVisitorsStack.pop_back();
		delete visitor;
	}
}

void MemoryLocality::callAdded(WorkQueueItem & item) {
	MemoryDependenceAnalysis * mda = 0;
	if (!item.function->isDeclaration()) {
		mda = &getAnalysisID<MemoryDependenceAnalysis>(&llvm::MemoryDependenceAnalysis::ID, *item.function);
	}
	LocalityFunctionVisitor * visitor = new LocalityFunctionVisitor(item, mda,
			&getAnalysis<llvm::AliasAnalysis>(),
			&getAnalysis<llvm::DataLayout>(),
			&getAnalysis<llvm::AllocIdentify>());
	visitor->start();
	if (visitor->isFinished) {
		delete visitor;
	} else {
		localityVisitorsStack.push_back(visitor);
	}
}

llvm::Function * MemoryLocality::getRoot(llvm::Module &M) const {
	llvm::Function * result = M.getFunction("main");
	if (result) {
		return result;
	}
	llvm::CallGraph * callGraph = &getAnalysis<llvm::CallGraph>();
	llvm::CallGraphNode * root = callGraph->getRoot();
	return root->getFunction();
}

void MemoryLocality::addEdge(const std::string & u, const std::string & v) {
	std::set<std::string> & dests = edges[u];
	dests.insert(v);
}

void MemoryLocality::print(llvm::raw_ostream &O, const llvm::Module *M) const {
	O << "digraph Locality {\n";
	for (EdgesType::const_iterator it = edges.begin(), ie = edges.end();
			it != ie; it++) {
		for (std::set<std::string>::const_iterator vit = it->second.begin(),
								vie = it->second.end();
				vit != vie; vit++) {
			O << "\t\"" << it->first << "\" -> \"" << *vit << "\";\n";
		}
	}
	O << "}\n";
}

char MemoryLocality::ID = 0;
static llvm::RegisterPass<MemoryLocality> _X(
		"memlocality",
		"Analyse memory locality in module",
		false, false);
}
