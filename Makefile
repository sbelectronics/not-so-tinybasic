all:
	gcc -c tbasic.c -o tbasic.o
	gcc -c -DLINUX host.c -o host.o
	gcc -o tbasic tbasic.o host.o
