CC = gcc
TARGETS = bin/monitor bin/tracer

SRCDIR = src
OBJDIR = obj
BINDIR = bin
TMPDIR = tmp

ARGS = execute -u "ls -l"

CFLAGS = -Wall -g -Iincludes
LIBS = -lm

.PHONY: all clean run-tracer run-monitor

all: folders $(TARGETS)

folders:
	mkdir -p $(SRCDIR) $(OBJDIR) $(BINDIR) $(TMPDIR)

$(BINDIR)/monitor: $(OBJDIR)/monitor.o
	$(CC) $(CFLAGS) $< -o $@ $(LIBS)

$(OBJDIR)/monitor.o: $(SRCDIR)/monitor.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR)/tracer: $(OBJDIR)/tracer.o
	$(CC) $(CFLAGS) $< -o $@ $(LIBS)

$(OBJDIR)/tracer.o: $(SRCDIR)/tracer.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR)/* $(TMPDIR)/* $(BINDIR)/{monitor,tracer}
	rm -f PIDS/*
	rm -f logs/*
	clear
	@echo Cleanup complete

rebuild:
	rm -rf $(OBJDIR)/* $(TMPDIR)/* $(BINDIR)/{monitor,tracer}
	$(MAKE) all
	clear
	@echo Rebuild complete

# run-monitor: $(BINDIR)/monitor
# 	./$<

# run-tracer: $(BINDIR)/tracer
# 	./$< $(ARGS)
