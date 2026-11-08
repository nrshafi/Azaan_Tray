CC ?= gcc
OPTFLAGS ?= -Os -ffunction-sections -fdata-sections -fno-asynchronous-unwind-tables
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror $(OPTFLAGS)
LDFLAGS ?= -Wl,--gc-sections -s -lm

BINDIR = bin

MKDIR_P = mkdir -p $(BINDIR)
RM_RF = rm -rf $(BINDIR) *.o
EXE = .exe

.PHONY: all test cli clean

all: test cli

test: $(BINDIR)/test_engine$(EXE)
	./$(BINDIR)/test_engine$(EXE)

$(BINDIR)/test_engine$(EXE): tests/test_engine.c engine/engine.c engine/engine.h
	@$(MKDIR_P)
	$(CC) $(CFLAGS) -Iengine tests/test_engine.c engine/engine.c -o $@ $(LDFLAGS)

cli: $(BINDIR)/azaan$(EXE)

$(BINDIR)/azaan$(EXE): cli/main.c engine/engine.c engine/engine.h
	@$(MKDIR_P)
	$(CC) $(CFLAGS) -Iengine cli/main.c engine/engine.c -o $@ $(LDFLAGS)

clean:
	@$(RM_RF)

