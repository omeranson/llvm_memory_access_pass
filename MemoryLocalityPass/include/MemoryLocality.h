#ifndef MEMORY_LOCALITY_H
#define MEMORY_LOCALITY_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include <llvm/Pass.h>

namespace MemoryLocality {
	typedef enum {
		PointerSource_Primitive,
		PointerSource_Local,
		PointerSource_Global,
		PointerSource_Argument,
		PointerSource_Function,
		PointerSource_Unknown
	} PointerSourceType;

	struct PointerSource {
		std::string name;
		PointerSourceType type;
		llvm::Argument * argument;

		PointerSource() : type(PointerSource_Unknown) {}
		PointerSource(const std::string & name, PointerSourceType type, llvm::Argument * argument) :
				name(name), type(type), argument(argument) {}

		void clear() {
			name.clear();
			type = PointerSource_Unknown;
			argument = 0;
		}
	};

	typedef std::map<std::string, std::set<std::string> > EdgesType;

	struct WorkQueueItem {
		std::set<llvm::Function *> callers;
		llvm::Function * function;
		std::vector<PointerSource> argumentSources;

		void clear() {
			callers.clear();
			function = 0;
			argumentSources.clear();
		}
	};
	typedef std::vector<WorkQueueItem> WorkQueueType;

	class LocalityFunctionVisitor;
	class MemoryLocality : public llvm::ModulePass {
	protected:
		EdgesType edges;
		std::vector<LocalityFunctionVisitor *> localityVisitorsStack;

		llvm::Function * getRoot(llvm::Module &M) const;
		void addEdge(const std::string & u, const std::string & v);
		void workOnItem(WorkQueueItem & item);
		void visit();
		void callAdded(WorkQueueItem & item);
	public:
		static char ID;
		MemoryLocality() : llvm::ModulePass(ID) {};
		virtual ~MemoryLocality() {};
		virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
		virtual bool runOnModule(llvm::Module &M);
		virtual void print(llvm::raw_ostream &O, const llvm::Module *M) const;
	};
}
#endif // MEMORY_LOCALITY_H
