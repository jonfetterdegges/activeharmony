TO_BASE=../../..

REQ_CFLAGS+=-fPIC
REQ_CPPFLAGS+=-I$(TO_BASE)/src
REQ_LDFLAGS+=$(SHAREDOBJ_FLAG)

SRCS=angel.c \
     exhaustive.c \
     libvertex.c \
     nm.c \
     pro.c \
     random.c \
     bandit.c \
     breadth_first.c \
     beam.c \
     queues.c

LIBEXEC_TGTS=angel.so \
             exhaustive.so \
             nm.so \
             pro.so \
             random.so \
             bandit.so \
             beam.so \
             breadth_first.so

angel.so: REQ_LDLIBS+=-lm
angel.so: libvertex.o

exhaustive.so: REQ_LDLIBS+=-lm
exhaustive.so: libvertex.o

nm.so: REQ_LDLIBS+=-lm
nm.so: libvertex.o

pro.so: REQ_LDLIBS+=-lm
pro.so: libvertex.o

random.so: REQ_LDLIBS+=-lm
random.so: libvertex.o

breadth_first.so: REQ_LDLIBS+=-lm
breadth_first.so: libvertex.o queues.o

beam.so: REQ_LDLIBS+=-lm
beam.so: libvertex.o queues.o

bandit.so: override LDLIBS+=-lm

# Active Harmony makefiles should always include this file last.
include $(TO_BASE)/make/common.mk
