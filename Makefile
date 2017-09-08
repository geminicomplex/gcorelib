CC = ${CROSS_COMPILE}gcc
INCLUDES := -I. -I../driver 
CFLAGS := ${DRIVER_CFLAGS} -c -Wall -Werror -fpic -D_FILE_OFFSET_BITS=64
LDFLAGS := -lgcore -shared -Wl,-soname,libgcore.so
EXECUTABLE = libgcore.so
HEADERS = $(wildcard ./*.h)
OBJECTS = $(wildcard ./*.o)

.PHONY : all
all: $(EXECUTABLE)

%.o : %.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@

 $(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $(EXECUTABLE) $(OBJECTS)

.PHONY : clean
clean :
	rm -f *.o libgcore.so
