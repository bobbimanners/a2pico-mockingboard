all: pulse-test

pulse-test: src/pulse-output.c src/ay-3-8913.c src/ay-3-8913.h src/wdc6522.c src/wdc6522.h
	gcc -Wall -g -o pulse-test src/pulse-output.c src/ay-3-8913.c src/wdc6522.c -lpulse -lpulse-simple

clean:
	rm -f *.o
	rm -f pulse-test

