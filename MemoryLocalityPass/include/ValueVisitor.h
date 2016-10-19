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
#define CALL_VISIT(type, value) do \
if (isa<type>(value)) { \
	return static_cast<T*>(this)->visit##type(cast<type>(value)); \
} while(0)

#define DEFINE_VISIT(type, parentType) \
void visit ## type (type & e) { \
	static_cast<T*>(this)->visit##parentType(e); \
}
	public:
		ValueVisitor(std::map<const Value *, RetType> & cache) :
				InstVisitor<T, RetType>() {}
		ValueVisitor() : InstVisitor<T, RetType>() {}

		void visitArgument(Argument & argument) {}
		void visitValue(Value & value) {}
		void visitConstantInt(ConstantInt & ci) {
			static_cast<T*>(this)->visitConstant(ci);
		}
		void visitConstantFP(ConstantFP & cfp) {
			static_cast<T*>(this)->visitConstant(cfp);
		}
		void visitGlobalValue(GlobalValue & globalValue) {
			static_cast<T*>(this)->visitConstant(globalValue);
		}
		void visitConstantExpr(ConstantExpr & constantExpr) {
			static_cast<T*>(this)->visitConstant(constantExpr);
		}
		DEFINE_VISIT(ConstantPointerNull, Constant)
		DEFINE_VISIT(UndefValue, Constant)
		void visitConstant(Constant & constant) {}

		RetType visit(Value * value) { return visit(*value); }
		RetType visit(Value & value) {
			if (isa<Instruction>(&value)) {
				Instruction & instruction =
						cast<Instruction>(value);
				return InstVisitor<T, RetType>::visit(instruction);
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
			if (isa<ConstantExpr>(&constant)) {
				ConstantExpr &constantExpr = cast<ConstantExpr>(constant);
				return visit(constantExpr);
			}
			CALL_VISIT(ConstantPointerNull, constant);
			CALL_VISIT(UndefValue, constant);
			return static_cast<T*>(this)->visitConstant(constant);
		}

		RetType visit(ConstantExpr * constantExpr) { return visit(*constantExpr); }
		RetType visit(ConstantExpr & constantExpr) {
			return static_cast<T*>(this)->visitConstantExpr(constantExpr);
		}
	};
}
#endif // VALUE_VISITOR_H
