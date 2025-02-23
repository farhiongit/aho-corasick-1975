CFLAGS = -Wall -pthread -Werror
#CFLAGS += -g
CFLAGS += -O3 -I..
#CC = cc -Wno-error=cpp
#CC = clang -I.. -Wno-error=\#warnings -fmacro-backtrace-limit=0
INCLUDES=../aho_corasick_template_impl.h ../aho_corasick_template.h

all: cloc execute speed

.PHONY: cloc
cloc:
	cloc --by-file $(INCLUDES)

.PHONY: execute
execute: aho_corasick_template_test
	nm --defined-only --extern-only aho_corasick_template_test
	time ./aho_corasick_template_test | tee ./aho_corasick_template_test.out

.PHONY: speed
speed: aho_corasick_template_speed_test ahoperftest
	time ./aho_corasick_template_speed_test
	time fgrep -c -e 1984 -e 1985 googlebooks-eng-all-1gram-20120701-0
	time ./ahoperftest

aho_corasick_template_test.o: aho_corasick_template_test.c $(INCLUDES)
aho_corasick_template_speed_test.o: aho_corasick_template_speed_test.c $(INCLUDES)
ahoperftest.o: ahoperftest.c $(INCLUDES)
