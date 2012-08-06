CFLAGS=-Wextra -Wall -I/opt/local/include
LDFLAGS=-L/opt/local/lib -ldvdread

dvdbackup: dvdbackup.o
	$(CC) -o $@ dvdbackup.o $(LDFLAGS)

dvdbackup.o: dvdbackup.c dvdbackup.h

clean:
	rm dvdbackup.o dvdbackup
