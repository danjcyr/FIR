fir1.o: fir1.h fir1.c
	gcc -Wall  -c  -o fir1.o fir1.c
fird:  fird.c fir1.o
	gcc -Wall -O3 -o fird fir1.c fird.c -lfftw3f -lm -lpthread
jfir: jfir.c fir1.o
	gcc -Wall  -o jfir fir1.c jfir.c -lfftw3f -lm -lpthread -ljack
