#ifndef MEMORY_ACCESS_CACHE_H
#define MEMORY_ACCESS_CACHE_H

namespace MemoryAccessPass {

class MemoryAccessInstVisitor;

class MemoryAccessCache {
public:
	virtual const MemoryAccessInstVisitor * getVisitor(llvm::Function *F) = 0;
};

template <class T>
class MemoryAccessCacheDuck : public MemoryAccessCache {
protected:
	T & m_duckImpl;
public:
	MemoryAccessCacheDuck(T & duckImpl) : m_duckImpl(duckImpl) {}
	virtual const MemoryAccessInstVisitor * getVisitor(llvm::Function *F) {
		return m_duckImpl.getVisitor(F);
	}
};

}

#endif // MEMORY_ACCESS_CACHE_H
