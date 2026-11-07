CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
LDFLAGS ?= -lm

.PHONY: all test cli clean

all: test cli

test: bin/test_engine
	./bin/test_engine

bin/test_engine: tests/test_engine.c engine/engine.c engine/engine.h
	@mkdir -p bin
	$(CC) $(CFLAGS) -Iengine tests/test_engine.c engine/engine.c -o bin/test_engine $(LDFLAGS)

cli: bin/azaan.exe

bin/azaan.exe: cli/main.c engine/engine.c engine/engine.h
	@mkdir -p bin
	$(CC) $(CFLAGS) -Iengine cli/main.c engine/engine.c -o bin/azaan.exe $(LDFLAGS)

clean:
	rm -rf bin *.o
