CFLAGS=-O3 -ffast-math -flto -fomit-frame-pointer -funroll-loops -fno-math-errno \
	   -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wundef -fno-common
LDLIBS=-lm

all: benchmark music-spectrum

music-spectrum: music-spectrum.o 12tet.o vqsdft.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS) -lasound

benchmark: benchmark.o 12tet.o vqsdft.o

pretty:
	clang-format -i --sort-includes *.[ch]

clean:
	rm -f *.o benchmark music-spectrum

.PHONY: all clean pretty
