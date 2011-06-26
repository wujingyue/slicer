PROG_NAMES = aget
PROGS_DIR = ../progs

PROGS = $(addprefix $(PROGS_DIR)/, $(PROG_NAMES))

all: $(PROGS:=-nocrit.bc)

%-nocrit.bc: %.bc
	opt -break-crit-edges < $< > $@

clean:
	rm -f $(PROGS:=-nocrit.bc)

.PHONY: clean
