all: pulse-test

pulse-test: pulse-output.c ay-3-8913.c ay-3-8913.h wdc6522.c wdc6522.h
	gcc -Wall -g -o pulse-test pulse-output.c ay-3-8913.c wdc6522.c -lpulse -lpulse-simple

clean:
	rm -f *.o
	rm -f pulse-test

