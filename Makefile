all:	usemem

usemem:	usemem.o
	cc -o usemem  usemem.o -lrt

clean:
	rm usemem *.o
