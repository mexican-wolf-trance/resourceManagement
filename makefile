C = gcc
CFLAGS = -Wall -g -std=gnu99
TARGET = oss
TARGET2 = user
OBJ1 = main.o
OBJ2 = executable.o
all: $(TARGET) $(TARGET2)
$(TARGET): $(OBJ1)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ1)
$(TARGET2): $(OBJ2)
	$(CC) $(CFLAGS) -o $(TARGET2) $(OBJ2)
main.o: main.c
	$(CC) $(CFLAGS) -c main.c
executable.o: executable.c
	$(CC) $(CFLAGS) -c executable.c

clean:
	/bin/rm -f *.o  $(TARGET) $(TARGET2) log.out

