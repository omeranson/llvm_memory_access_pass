#ifndef VALUE_VISITOR_H
#define VALUE_VISITOR_H

#include <map>

#include <llvm/InstVisitor.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/raw_ostream.h>

namespace llvm {
	template <typename T, typename RetType=void>
	class ValueVisitor : public InstVisitor<T, RetType> {
	protected:	
		std::map<const Value *, RetType> & m_cache;
	public:
		ValueVisitor(std::map<const Value *, RetType> & cache) :
				InstVisitor<T, RetType>(), m_cache(cache) {}
		ValueVisitor() : InstVisitor<T, RetType>(), m_cache(*(new std::map<const Value *, RetType>())) {}

		void visitArgument(Argument & argument) {}
		void visitValue(Value & value) {}
		void visitConstantInt(ConstantInt & ci) {}
		void visitConstantFP(ConstantFP & cfp) {}
		void visitGlobalValue(GlobalValue & globalValue) {}
		void visitConstant(Constant & constant) {}
		void visitConstantExpr(ConstantExpr & constantExpr) {}

		RetType visit(Value * value) { return visit(*value); }
		RetType visit(Value & value) {
			typename std::map<const Value *, RetType>::iterator it =
					m_cache.find(&value);
			if (it != m_cache.end()) {
				return it->second;
			}
			if (isa<Instruction>(&value)) {
				Instruction & instruction =
						cast<Instruction>(value);
				return static_cast<InstVisitor<T, RetType>*>(this)->visit(instruction);
			}
			if (isa<Constant>(&value)) {
				Constant & constant = cast<Constant>(value);
				return visit(constant);
			}
			if (isa<Argument>(value)) {
				Argument & argument = cast<Argument>(value);
				return static_cast<T*>(this)->visitArgument(argument);
			}
			return static_cast<T*>(this)->visitValue(value);
		}

		RetType visit(Constant *constant) { return visit(*constant); }
		RetType visit(Constant &constant) {
			if (isa<ConstantInt>(&constant)) {
				ConstantInt &constantInt = cast<ConstantInt>(constant);
				return static_cast<T*>(this)->visitConstantInt(constantInt);
			}
			if (isa<ConstantFP>(&constant)) {
				ConstantFP &constantFP = cast<ConstantFP>(constant);
				return static_cast<T*>(this)->visitConstantFP(constantFP);
			}
			if (isa<GlobalValue>(&constant)) {
				GlobalValue &globalValue = cast<GlobalValue>(constant);
				return static_cast<T*>(this)->visitGlobalValue(globalValue);
			}
			if (isa<GlobalValue>(&constant)) {
				GlobalValue &globalValue = cast<GlobalValue>(constant);
				return static_cast<T*>(this)->visitGlobalValue(globalValue);
			}
			if (isa<ConstantExpr>(&constant)) {
				ConstantExpr &constantExpr = cast<ConstantExpr>(constant);
				return visit(constantExpr);
			}
			return static_cast<T*>(this)->visitConstant(constant);
		}

		RetType visit(ConstantExpr * constantExpr) { return visit(*constantExpr); }
		RetType visit(ConstantExpr & constantExpr) {
			return static_cast<T*>(this)->visitConstantExpr(constantExpr);
		}
	};
}
#endif // VALUE_VISITOR_H
