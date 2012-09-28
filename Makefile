all: test

test: shorten.o test.o
shorten.o: shortnames.h
test.o: shortnames.h
clean:
	-rm -f *.o test
