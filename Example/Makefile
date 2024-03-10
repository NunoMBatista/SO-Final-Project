#Autores: \
  João Filipe Sequeira Montenegro Nº 2016228672 \
  João Miguel Rainho Mendes Nº 2016230975


CFLAGS = -c -g -Wall -D_REENTRANT -pthread
CFLAGSF = -g -Wall -D_REENTRANT -pthread
CC = gcc

final: ficheiro.o main.o triagem.o doutor.o log.o sinais.o
	$(CC) ficheiro.o main.o triagem.o doutor.o log.o sinais.o -o final $(CFLAGSF)
main.o: main.c ficheiro.h triagem.h global.h doutor.h log.h sinais.h
	$(CC) main.c $(CFLAGS)
ficheiro.o: ficheiro.c global.h
	$(CC) ficheiro.c $(CFLAGS)
triagem.o: triagem.c global.h
	$(CC) triagem.c $(CFLAGS)
doutor.o:	doutor.c global.h
	$(CC) doutor.c $(CFLAGS)
log.o: log.c global.h
	$(CC) log.c $(CFLAGS)
sinais.o: sinais.c global.h
	$(CC) sinais.c $(CFLAGS)
clean:
	rm *.o final input_pipe log.txt
