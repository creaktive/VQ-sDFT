CFLAGS=-O3 -ffast-math -flto -fomit-frame-pointer -funroll-loops -fno-math-errno \
	   -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wundef -fno-common
LDLIBS=-lm

all: benchmark

benchmark: benchmark.o 12tet.o vqsdft.o

pretty:
	clang-format -i --sort-includes *.[ch]

clean:
	rm -f *.o benchmark

.PHONY: all clean pretty
