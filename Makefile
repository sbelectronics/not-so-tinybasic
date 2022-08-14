all:
	gcc -c tbasic.c -o tbasic.o
	gcc -c -DLINUX host.c -o host.o
	gcc -o tbasic tbasic.o host.o

up:
	rm -rf holding
	mkdir holding
	cp tbasic.c host.c host.h inout.8kn holding/
	python ~/projects/pi/z8000/cpm8kdisks/addeof.py holding/*.c holding/*.h holding/*.8kn
	cpmrm -f cpm8k ~/projects/pi/z8000/super/sup.img tbasic.c host.c host.h inout.8kn || true
	cpmcp -f cpm8k ~/projects/pi/z8000/super/sup.img holding/* 0:

.PHONY: down
down:
	mkdir -p down
	rm -f down/TBASIC.Z8K
	cpmcp -f cpm8k ~/projects/pi/z8000/super/sup.img 0:TBASIC.Z8K down/

listimg:
	cpmls -f cpm8k -D ~/projects/pi/z8000/super/sup.img
