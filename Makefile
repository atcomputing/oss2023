all:	attract countcat pad usecpu usemem

usemem:	usemem.o
	cc         -o usemem  usemem.o -lrt
	cc -static -o usemems usemem.o -lrt

clean:
	rm attract countcat pad usecpu usemem *.o
