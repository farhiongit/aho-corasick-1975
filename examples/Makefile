CPPFLAGS += -I..
CFLAGS += -O3
#CFLAGS += -g
DIRMAPS = ../../minimaps  # see https://github.com/farhiongit/minimaps
#TOOL = valgrind --leak-check=full
#TOOL = ddd

all: aho_corasick_generic_test test
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:$(DIRMAPS) $(TOOL) ./aho_corasick_generic_test
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:$(DIRMAPS) ./test

.INTERMEDIATE: ../aho_corasick.o
# When NMEYER_85 is defined, the incremental string matching (Meyer, 1985) is not used. The original Aho-Corasick algorithm is used.
#../aho_corasick.o: CPPFLAGS += -DNMEYER_85
../aho_corasick.o: CPPFLAGS += -I$(DIRMAPS)
../aho_corasick.o: CFLAGS += -std=c11

aho_corasick_generic_test: LDFLAGS += -L$(DIRMAPS)
aho_corasick_generic_test: LDLIBS += -lmap
aho_corasick_generic_test: aho_corasick_generic_test.c ../aho_corasick.o

test: LDFLAGS += -L$(DIRMAPS)
test: LDLIBS += -lmap
test: test.c ../aho_corasick.o
