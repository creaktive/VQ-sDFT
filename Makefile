CFLAGS   = -O3 -ffast-math -flto -fno-common -fno-math-errno -fomit-frame-pointer -funroll-loops \
           -Wall -Wconversion -Wextra -Wno-deprecated-declarations -Wpedantic -Wshadow -Wundef \
           -MMD -MP
LDLIBS   = -lm

TARGETS  = benchmark doorbell music-spectrum
SRCS     = $(wildcard *.c)
DEPS     = $(SRCS:.c=.d)

doorbell:       LDLIBS += -lasound
music-spectrum: LDLIBS += -lasound

.PHONY: all clean pretty

all: $(TARGETS)

doorbell:       doorbell.o vqsdft.o
benchmark:      benchmark.o 12tet.o vqsdft.o
music-spectrum: music-spectrum.o 12tet.o vqsdft.o

pretty:
	clang-format -i --sort-includes *.[ch]

clean:
	rm -f *.d *.o $(TARGETS)

-include $(DEPS)
