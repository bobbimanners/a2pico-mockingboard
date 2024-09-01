all: pacat-simple pulse-test

pacat-simple: pacat-simple.c
	gcc -o pacat pacat-simple.c -lpulse -lpulse-simple

pulse-test: pulse-output.c ay-3-8913.c ay-3-8913.h
	gcc -g -o pulse-test pulse-output.c ay-3-8913.c -lpulse -lpulse-simple

