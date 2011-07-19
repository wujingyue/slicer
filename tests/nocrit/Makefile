PROG_NAMES = aget FFT RADIX pbzip2 CHOLESKY
PROGS_DIR = ../progs

PROGS = $(addprefix $(PROGS_DIR)/, $(PROG_NAMES))

all: $(PROGS:=-nocrit.bc)

# Use -o when possible
# <opt> will remove the output file on failure.
%-nocrit.bc: %.bc
	opt -break-crit-edges -o $@ < $<

clean:
	rm -f $(PROGS:=-nocrit.bc)

.PHONY: clean
