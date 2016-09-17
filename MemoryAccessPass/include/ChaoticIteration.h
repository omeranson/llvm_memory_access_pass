#ifndef CHAOTIC_ITERATION_H
#define CHAOTIC_ITERATION_H

#include <list>

#include <llvm/InstVisitor.h>
#include <llvm/Support/raw_ostream.h>

namespace MemoryAccessPass {

	class Join {
		public:
		bool join(const llvm::BasicBlock * from, const llvm::BasicBlock * to);
	};

	class BasicBlockInFunctionComparator {
	private:
		llvm::Function & m_function;
	public:
		BasicBlockInFunctionComparator(llvm::Function & function): m_function(function) {}
		bool operator()(llvm::BasicBlock * left, llvm::BasicBlock * right) const {
			if (left == right) {
				return false;
			}
			int left_idx = -1;
			int right_idx = -1;
			int idx = 0;
			for (llvm::Function::const_iterator it = m_function.begin(),
								ie = m_function.end();
					it != ie; it++) {
				if (left == it) {
					left_idx = idx;
				}
				if (right == it) {
					right_idx = idx;
				}
				idx++;
			}
			return (left_idx < right_idx);
		}
	};

	template <class T> class ChaoticIteration {
	private:
		T & m_visitor;
	protected:
		void populateWorklistWithSuccessors(
				std::list<llvm::BasicBlock *> & worklist,
				const llvm::BasicBlock & element) {
			const llvm::TerminatorInst * terminator = element.getTerminator();
			int successorCount = terminator->getNumSuccessors();
			T & visitor = getVisitor();
			for (int idx = 0; idx < successorCount; idx++) {
				llvm::BasicBlock * BB = terminator->getSuccessor(idx);
				if (visitor.join(&element, BB)) {
					llvm::errs() << "Adding to worklist: " << BB->getName() << ": " << element.getName() << " -> " << BB->getName() << "\n";
					worklist.push_back(BB);
				}
			}
		}
		T & getVisitor() { return m_visitor; }

	public:
		ChaoticIteration<T>(T & visitor) : m_visitor(visitor) {};
		void iterate(llvm::Function * F) { return iterate(*F); }
		void iterate(llvm::Function & F) {
			getVisitor().visitFunction(F);
			return iterate(F.getEntryBlock());
		}
		void iterate(llvm::BasicBlock * BB) { return iterate(*BB); }
		void iterate(llvm::BasicBlock & BB) {
			BasicBlockInFunctionComparator comparator(*BB.getParent());
			std::list<llvm::BasicBlock *> worklist;
			llvm::errs() << "Adding to worklist: " << BB.getName() << "\n";
			worklist.push_back(&BB);
			T & visitor = getVisitor();
			while (!worklist.empty()) {
				std::list<llvm::BasicBlock *>::iterator
						first_element = worklist.begin();
				worklist.pop_front();
				llvm::BasicBlock * element = *first_element;
				llvm::errs() << "Working on: " << element->getName() << "\n";
				visitor.visit(element);
				populateWorklistWithSuccessors(worklist, *element);
				worklist.sort(comparator);
				worklist.unique();
			}
		}
	};
}
#endif // CHAOTIC_ITERATION_H
