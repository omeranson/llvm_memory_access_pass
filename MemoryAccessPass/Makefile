BASE = MemoryAccess MemoryAccessInstVisitor
OBJS = $(foreach BASEFILE,$(BASE),src/$(BASEFILE).o)
#OBJS = src/MemoryAccess.o src/MemoryAccessInstVisitor.o
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

%.o: %.c
	@ echo '[CC]	[$^]	[$@]'
	@ ${CC} -c -o $@ $^ ${CXXFLAGS}

%.o: %.cpp
	@ echo '[CXX]	[$^]	[$@]'
	@ ${CXX} -c -o $@ $^ ${CXXFLAGS}

clean:
	@ echo '[RM]	[${OBJS}]'
	@ rm -f ${OBJS}
