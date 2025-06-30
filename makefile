all:
	make -f makelib.mk
	make -f makeinitksocket.mk
	make -f makeuser.mk

clean:
	make clean -f makelib.mk
	make clean -f makeinitksocket.mk
	make clean -f makeuser.mk
	-rm -f file2.txt
