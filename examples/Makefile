CPPFLAGS += -I..
CFLAGS += -O3

all: aho_corasick_generic_test
	./aho_corasick_generic_test

.SECONDARY: ../aho_corasick.o
../aho_corasick.o: CFLAGS += -std=c11
aho_corasick_generic_test: aho_corasick_generic_test.c ../aho_corasick.o
