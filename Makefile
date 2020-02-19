CC=gcc
CFLAGS= -I./ -Wall -Wextra -Wno-unused-function -pedantic
DEPFLAGS = -MT $@ -MMD -MP -MF $*.d
LIB_DST=libppmtools.a

ifdef OPTF_DEBUG
OPT=-O0 -g3
else
OPT=-O3
endif

CFLAGS += $(OPT)

OBJ_SRC=TaskSegRaw.o task_classifier.o arll.o model_parser.o TaskSegBuck.o gplot.o pm.o graph_miner.o TaskSeg.o element_context.o seg_cluster.o abstract_utils.o
OBJ=$(PROG).o $(OBJ_SRC)

all: $(LIB_DST)

$(LIB_DST): $(OBJ_SRC)
	ar rcs $@ $^

%.o: %.c %.d
	$(CC) $(DEPFLAGS) -c $(CFLAGS) -o $@ $<

DEPFILES := $(OBJ:%.o=%.d)

$(DEPFILES):

include $(wildcard $(DEPFILES))

clean:
	@$(RM) -vrf $(OBJ) $(LIB_DST) $(OBJ:.o=.d)
