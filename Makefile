# NAME: Jesse Joseph
# EMAIL: jjoseph@hmc.edu
# ID: 040161840

GXX=gcc

lab1: lab1.c lab1.h
	$(GXX) lab1.c -o lab1a -lpthread -Wall -Wpedantic -Wextra

clean:
	-rm lab1a
	-rm lab1a-040161840.tar.gz

dist:
	tar -cvzf lab1a-040161840.tar.gz lab1.c lab1.h Makefile README
