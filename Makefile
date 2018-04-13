all: cleandefrag defrag clean
debug: cleandefrag defragdebug clean

defragdebug:
	gcc -g -DDEBUG -ggdb -lm -Wall -c defrag.c
	gcc -g -ggdb -lm -o defrag defrag.o

defrag:
	gcc -g -ggdb -lm -Wall -c defrag.c
	gcc -g -ggdb -lm -o defrag defrag.o

clean:
	rm -rf *.o *.gch *.dSYM

cleandefrag:
	rm -rf *.o *.gch *.dSYM *defrag\ * *defrag