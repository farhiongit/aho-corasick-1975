CFLAGS = -Wall -O3 -pthread -Werror
#CFLAGS += -g
CC = cc -Wno-error=cpp
#CC = clang -Wno-error=\#warnings

all: cloc execute aho_corasick.o nmo speed

aho_corasick_test: CFLAGS += -DACM_ASSOCIATED_VALUE
aho_corasick_test: aho_corasick_test.c

aho_corasick-template_test: aho_corasick_template_test.c

.PHONY: cloc
cloc:
	cloc --by-file-by-lang aho_corasick_test.c aho_corasick.c aho_corasick.h aho_corasick_template_test.c aho_corasick_template.h aho_corasick_template_impl.h

.PHONY: execute
execute: aho_corasick_test aho_corasick_template_test
	nm --defined-only --extern-only aho_corasick_test
	time ./aho_corasick_test
	nm --defined-only --extern-only aho_corasick_template_test
	time ./aho_corasick_template_test

.PHONY: nmo
nmo:
	nm --defined-only --extern-only aho_corasick.o

aho_corasick.o: CFLAGS += -DACM_ASSOCIATED_VALUE -DACM_SYMBOL='long long int'
aho_corasick.o: aho_corasick.c aho_corasick.h

.PHONY: speed
speed: aho_corasick_template_speed_test
	time ./aho_corasick_template_speed_test
	time fgrep -c -e 1984 -e 1985 googlebooks-eng-all-1gram-20120701-0
