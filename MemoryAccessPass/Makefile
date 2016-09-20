BASE = MemoryAccess MemoryAccessInstVisitor
OBJS = $(foreach BASEFILE,$(BASE),src/$(BASEFILE).o)
INCS = $(foreach BASEFILE,$(BASE),include/$(BASEFILE).h) include/ChaoticIteration.h include/ValueVisitor.h
INCLUDES = $(foreach BASEFILE,$(BASE),include/$(BASEFILE).h)

LLVM_INSTALL?=${HOME}/opt/llvm-install
CXXFLAGS=$(shell ${LLVM_INSTALL}/bin/llvm-config --cxxflags)
CXXFLAGS+= -Iinclude -fPIC -g
LDFLAGS=$(shell ${LLVM_INSTALL}/bin/llvm-config --ldflags)
LDFLAGS+= -shared -fPIC
CC=${LLVM_INSTALL}/bin/clang
CXX=${LLVM_INSTALL}/bin/clang++

all: libmemaccess.so

libmemaccess.so: ${OBJS}
	@ echo '[LD]	[$^]	[$@]'
	@ ${CXX} -Wl,-soname,$@ -o $@ $^ ${LDFLAGS}

%.o: %.c ${INCS}
	@ echo '[CC]	[$<]	[$@]'
	@ ${CC} -c -o $@ $< ${CXXFLAGS}

%.o: %.cpp ${INCS}
	@ echo '[CXX]	[$<]	[$@]'
	@ ${CXX} -c -o $@ $< ${CXXFLAGS}

clean:
	@ echo '[RM]	[${OBJS}]'
	@ rm -f ${OBJS}
