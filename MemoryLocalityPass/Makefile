BASE = MemoryLocality
TARGET=libmemlocality.so
OBJS = $(foreach BASEFILE,$(BASE),src/$(BASEFILE).o)
INCS = $(foreach BASEFILE,$(BASE),include/$(BASEFILE).h) include/ValueVisitor.h include/MemoryDependenceAnalysis.h
INCLUDES = $(foreach BASEFILE,$(BASE),include/$(BASEFILE).h)

LLVM_INSTALL?=${HOME}/opt/llvm-install
CXXFLAGS=$(shell ${LLVM_INSTALL}/bin/llvm-config --cxxflags)
CXXFLAGS+= -I/home/oanson/projects/poolalloc.git/include
CXXFLAGS+= -Iinclude -fPIC -g -O0
LDFLAGS=$(shell ${LLVM_INSTALL}/bin/llvm-config --ldflags)
LDFLAGS+= -shared -fPIC
CC=${LLVM_INSTALL}/bin/clang
CXX=${LLVM_INSTALL}/bin/clang++

all: ${TARGET}

${TARGET}: ${OBJS}
	@ echo '[LD]	[$^]	[$@]'
	@ ${CXX} -Wl,-soname,$@ -o $@ $^ ${LDFLAGS}

%.o: %.c ${INCS}
	@ echo '[CC]	[$<]	[$@]'
	@ ${CC} -c -o $@ $< ${CXXFLAGS}

%.o: %.cpp ${INCS}
	@ echo '[CXX]	[$<]	[$@]'
	@ ${CXX} -c -o $@ $< ${CXXFLAGS}

clean:
	@ echo '[RM]	[${TARGET} ${OBJS}]'
	@ rm -f ${TARGET} ${OBJS}
