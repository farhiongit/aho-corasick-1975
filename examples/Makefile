CFLAGS = -Wall -O3 -pthread -Werror
#CFLAGS += -g
#CC = cc -Wno-error=cpp
CC = clang -I.. -Wno-error=\#warnings -fmacro-backtrace-limit=0

all: cloc execute speed

.PHONY: cloc
cloc:
	cloc --by-file ../aho_corasick_template.h ../aho_corasick_template_impl.h

.PHONY: execute
execute: aho_corasick_template_test
	nm --defined-only --extern-only aho_corasick_template_test
	time ./aho_corasick_template_test | tee ./aho_corasick_template_test.out

.PHONY: speed
speed: aho_corasick_template_speed_test ahoperftest
	time ./aho_corasick_template_speed_test
	time fgrep -c -e 1984 -e 1985 googlebooks-eng-all-1gram-20120701-0
	time ./ahoperftest
