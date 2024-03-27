CC = gcc
CFLAGS = -I. -pthread -g -Wall -Wextra
DEPS = global.h system_functions.h
OBJ = mobile_user.o backoffice_user.o system_functions.o system_manager.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: mobile_user backoffice_user 5g_auth_platform

mobile_user: mobile_user.o
	$(CC) -o $@ $^ $(CFLAGS)

backoffice_user: backoffice_user.o
	$(CC) -o $@ $^ $(CFLAGS)

5g_auth_platform: system_manager.o system_functions.o
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f *.o mobile_user backoffice_user 5g_auth_platform