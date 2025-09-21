CFILES += ../util/scf_rbtree.c
CFILES += scf_coroutine.c
CFILES += scf_coroutine_run.c
#CFILES += main.c
CFILES += scf_coroutine_asm.S

CFLAGS += -g -O3 -fPIC -shared
CFLAGS += -I../util

LDFLAGS +=

all:
	gcc $(CFLAGS) $(CFILES) $(LDFLAGS) -o libscf_co.so
	gcc -I../util main.c -lscf_co -L./
