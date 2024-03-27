CC=gcc
CFLAGS=-Wall -Wextra -pthread
BINDIR=/usr/local/bin
TARGET1=mobile_user
TARGET2=backoffice
TARGET3=system_manager

all: $(TARGET1) $(TARGET2) $(TARGET3)

$(TARGET1): $(TARGET1).c
	$(CC) $(CFLAGS) $(TARGET1).c -o $(TARGET1) 

$(TARGET2): $(TARGET2).c
	$(CC) $(CFLAGS) $(TARGET2).c -o $(TARGET2) 

$(TARGET3): $(TARGET3).c
	$(CC) $(CFLAGS) $(TARGET3).c -o $(TARGET3)