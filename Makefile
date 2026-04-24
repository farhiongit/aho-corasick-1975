CPPFLAGS += -I..
CFLAGS += -O3
CFLAGS+=-fPIC
DIRMAPS = ../minimaps  # see https://github.com/farhiongit/minimaps

CPPFLAGS += -I$(DIRMAPS)
LDFLAGS += -L$(DIRMAPS)
LDLIBS += -lmap

all: libaho_corasick.so

lib%.so: LDFLAGS+=-shared
lib%.so: %.o
	$(CC) $(LDFLAGS) -o "$@" "$^"
