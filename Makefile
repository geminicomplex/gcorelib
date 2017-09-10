CC = ${CROSS_COMPILE}gcc
INCLUDES := -I. -I../driver 
CFLAGS := ${DRIVER_CFLAGS} -c -Wall -Werror -fpic -D_FILE_OFFSET_BITS=64
LDFLAGS := -shared -Wl,-soname,libgcore.so
EXECUTABLE = libgcore.so

SOURCES = $(wildcard ./*.c)
OBJECTS = $(SOURCES:%.c=%.o)
HEADERS = $(wildcard ./*.h)

.PHONY : all
all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(INCLUDES) $(LDFLAGS) $(OBJECTS) -o $(EXECUTABLE)

%.o : %.c $(HEADERS)
	$(CC) $(INCLUDES) $(CFLAGS) $< -o $@


.PHONY : clean
clean :
	rm -f *.o libgcore.so
