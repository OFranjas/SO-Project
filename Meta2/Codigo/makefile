CFLAGS = -g -Wall -pthread -Wextra
CC = gcc
OBJS1 = SystemManager.o Config.o
PROG1 = SystemManager
OBJS2 = Sensor.o
PROG2 = Sensor
OBJS3 = UserConsole.o
PROG3 = UserConsole

all: $(PROG1) $(PROG2) $(PROG3) clean

$(PROG1): $(OBJS1)
	$(CC) $(CFLAGS) -o $(PROG1) $(OBJS1)

$(PROG2): $(OBJS2)
	$(CC) $(CFLAGS) -o $(PROG2) $(OBJS2)

$(PROG3): $(OBJS3)
	$(CC) $(CFLAGS) -o $(PROG3) $(OBJS3)

SystemManager.o: src/SystemManager.c
	$(CC) $(CFLAGS) -c src/SystemManager.c

Config.o: src/Config.c
	$(CC) $(CFLAGS) -c src/Config.c

Sensor.o: src/Sensor.c
	$(CC) $(CFLAGS) -c src/Sensor.c

UserConsole.o: src/UserConsole.c
	$(CC) $(CFLAGS) -c src/UserConsole.c

clean:
	rm -f *.o
