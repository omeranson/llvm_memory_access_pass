#ifndef CHAOTIC_ITERATION_H
#define CHAOTIC_ITERATION_H

#include <llvm/InstVisitor.h>
#include <llvm/Support/raw_ostream.h>

namespace MemoryAccessPass {

	class Join {
		public:
		bool join(const llvm::BasicBlock * from, const llvm::BasicBlock * to);
	};

	template <class T> class ChaoticIteration {
	private:
		T & m_visitor;
	protected:
		void populateWorklistWithSuccessors(
				std::vector<llvm::BasicBlock *> & worklist,
				const llvm::BasicBlock & element) {
			const llvm::TerminatorInst * terminator = element.getTerminator();
			int successorCount = terminator->getNumSuccessors();
			T & visitor = getVisitor();
			for (int idx = 0; idx < successorCount; idx++) {
				llvm::BasicBlock * BB = terminator->getSuccessor(idx);
				llvm::errs() << "Adding basic block to worklist: " << BB->getName() << "\n";
				if (visitor.join(&element, BB)) {
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
			std::vector<llvm::BasicBlock *> worklist;
			worklist.push_back(&BB);
			T & visitor = getVisitor();
			while (!worklist.empty()) {
				std::vector<llvm::BasicBlock *>::iterator
						first_element = worklist.begin();
				llvm::BasicBlock * element = *first_element;
				llvm::errs() << "Chaotic iteration on basic block: " << element->getName() << "\n";
				visitor.visit(element);
				worklist.erase(first_element);
				populateWorklistWithSuccessors(worklist, *element);
			}
		}
	};
}
#endif // CHAOTIC_ITERATION_H
