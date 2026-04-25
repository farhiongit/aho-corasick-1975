CFLAGS += -O3
CFLAGS+=-fPIC
DIRMAPS = ../minimaps  # see https://github.com/farhiongit/minimaps

all: libac75.so

.INTERMEDIATE: aho_corasick.o
aho_corasick.o: CPPFLAGS += -I$(DIRMAPS)

libac75.so: LDFLAGS+=-shared
libac75.so: aho_corasick.o
	$(CC) $(LDFLAGS) -o "$@" "$^"
