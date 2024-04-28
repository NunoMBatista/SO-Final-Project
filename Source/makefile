CC = gcc
CFLAGS = -I. -pthread -g -Wall -Wextra
DEPS = global.h system_functions.h queue.h
OBJ = mobile_user.o backoffice_user.o system_functions.o system_manager.o queue.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: mobile_user backoffice_user 5g_auth_platform

mobile_user: mobile_user.o queue.o
	$(CC) -o $@ $^ $(CFLAGS)

backoffice_user: backoffice_user.o queue.o
	$(CC) -o $@ $^ $(CFLAGS)

5g_auth_platform: system_manager.o system_functions.o queue.o
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f *.o mobile_user backoffice_user 5g_auth_platform