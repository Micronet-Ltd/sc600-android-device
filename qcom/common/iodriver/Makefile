LDFLAGS=-pthread -lpthread
CUSTOMDEFINES = LINUX_BUILD
CFLAGS=-std=c99 -pthread -DUSE_THREADS -Wall -g -O0 -D$(CUSTOMDEFINES)

OBJECTS := iodriver.o control.o accel.o util.o tty.o queue.o frame.o j1708.o
TARGET := iodriver



$(TARGET): $(OBJECTS)


clean:
	-rm $(OBJECTS) $(TARGET)
