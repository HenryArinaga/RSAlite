CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g

CLI_OBJS   = main.o factor.o prime.o log.o simd.o
GUI_OBJS   = gui_main.o factor.o prime.o log.o simd.o
BENCH_OBJS = bench.o factor.o prime.o simd.o
TEST_OBJS  = test_factor.o factor.o prime.o simd.o
LOG_TEST_OBJS = log_test.o log.o factor.o prime.o simd.o

all: rsalite rsalite-gui rsalite-bench

rsalite: $(CLI_OBJS)
	$(CC) $(CFLAGS) -o $@ $(CLI_OBJS)

rsalite-gui: $(GUI_OBJS)
	$(CC) $(CFLAGS) -o $@ $(GUI_OBJS) $(GTK_LIBS)

rsalite-bench: $(BENCH_OBJS)
	$(CC) $(CFLAGS) -o $@ $(BENCH_OBJS)

main.o: main.c factor.h prime.h simd.h
	$(CC) $(CFLAGS) -c $<

gui_main.o: gui_main.c factor.h prime.h simd.h
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -c $<

factor.o: factor.c factor.h prime.h optimization.h simd.h
	$(CC) $(CFLAGS) -c $<

prime.o: prime.c prime.h
	$(CC) $(CFLAGS) -c $<

simd.o: simd.c simd.h
	$(CC) $(CFLAGS) -c $<

log.o: log.c log.h
	$(CC) $(CFLAGS) -c $<

bench.o: bench.c factor.h prime.h simd.h
	$(CC) $(CFLAGS) -c $<

test_factor.o: test_factor.c factor.h prime.h simd.h
	$(CC) $(CFLAGS) -c $<

log_test.o: log_test.c log.h factor.h prime.h simd.h
	$(CC) $(CFLAGS) -c $<

GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS   := $(shell pkg-config --libs gtk+-3.0 2>/dev/null)

test: test_factor

test_factor: $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJS)

log_test: $(LOG_TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $(LOG_TEST_OBJS)

clean:
	rm -f *.o rsalite rsalite-gui rsalite-bench test_factor log_test

.PHONY: all clean test
