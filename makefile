make: blockio.c
		gcc -pthread -g blockio.c -Wall

run:
		./a.out

test:
		clear
		clear
		clear
		valgrind --tool=helgrind ./a.out
