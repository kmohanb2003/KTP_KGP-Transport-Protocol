all:
	gcc -Wall -L. -o user1 user1.c -lksocket
	gcc -Wall -L. -o user2 user2.c -lksocket

clean:
	-rm -f user1 user2
