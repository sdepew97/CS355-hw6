all: cleandefrag defrag clean

defrag:
	gcc -g -ggdb -Wall -c defrag.c
	gcc -g -ggdb -o defrag defrag.o

clean:
	rm -rf *.o *.gch *.dSYM

cleandefrag:
	rm -rf *.o *.gch *.dSYM *defrag\ * *defrag