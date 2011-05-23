all: display

display: display.cpp
	g++ $< -o $@ -g -O2 -MMD

clean:
	rm -f display *.d

-include *.d
