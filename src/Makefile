CFLAGS=-Wall -I/opt/local/include
LDFLAGS=-L/opt/local/lib -ldvdread

dvdbackup: dvdbackup.o
	$(CC) -o $@ dvdbackup.o $(LDFLAGS)

clean:
	rm dvdbackup.o dvdbackup
