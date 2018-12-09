CC = gcc
CFLAGS = -std=c11 -g -O0
MEMORY_TRACER_OBJ = src/memory-tracer.o src/tracing.o src/utils.o src/perf.o src/perf-handler.o src/ftrace.o src/ftrace-handler.o

all: memory-tracer

utils.o: src/utils.c src/utils.h
tracing.o: src/tracing.c src/memory-tracer.h src/tracing.h src/utils.h
ftrace.o: src/ftrace.c src/memory-tracer.h src/ftrace.h
ftrace-handler.o: src/ftrace-handler.c src/memory-tracer.h src/tracing.h src/utils.h src/ftrace.h
perf.o: src/perf.c src/memory-tracer.h src/perf.h
perf-handler.o: src/perf-handler.c src/memory-tracer.h src/tracing.h src/utils.h src/perf.h
memory-tracer.o: src/memory-tracer.c src/perf-handler.h src/tracing.h src/utils.h src/ftrace-handler.h src/memory-tracer.h

memory-tracer: $(MEMORY_TRACER_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(MEMORY_TRACER_OBJ)

.PHONY: clean
clean:
	rm -f $(MEMORY_TRACER_OBJ) memory-tracer
