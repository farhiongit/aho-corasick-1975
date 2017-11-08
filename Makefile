CFLAGS = -Wall -O -pthread
#CFLAGS += -g
#CFLAGS += -pedantic
#CC = clang

all: aho_corasick_test cloc execute aho_corasick.o nmo

aho_corasick_test: CFLAGS += -DACM_ASSOCIATED_VALUE
aho_corasick_test: aho_corasick_test.c

.PHONY: cloc
cloc:
	cloc --by-file-by-lang aho_corasick_test.c aho_corasick.c aho_corasick.h

.PHONY: execute
execute:
	nm --defined-only --extern-only aho_corasick_test
	./aho_corasick_test

.PHONY: nmo
nmo:
	nm --defined-only --extern-only aho_corasick.o

aho_corasick.o: CFLAGS += -DACM_ASSOCIATED_VALUE
