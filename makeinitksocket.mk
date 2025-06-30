all:
	gcc -Wall -L. -o initksocket -DVERBOSE initksocket.c -lksocket

clean:
	-rm -f initksocket
