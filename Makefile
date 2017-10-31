CC = ${CROSS_COMPILE}gcc
INCLUDES := -I. -I../driver 
CFLAGS := ${DRIVER_CFLAGS} -ggdb -c -Wall -Werror -fpic -D_FILE_OFFSET_BITS=64
LDFLAGS := -ggdb -shared -Wl,-soname,libgcore.so
EXECUTABLE = libgcore.so

SRCS = $(wildcard ./*.c)
OBJS = $(SRCS:%.c=%.o)
HEADERS = $(SRCS:%.c=%.h)

.PHONY : all
all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJS)
	$(CC) $(INCLUDES) $(LDFLAGS) $(OBJS) -o $(EXECUTABLE)

%.o : %.c $(HEADERS)
	$(CC) $(INCLUDES) $(CFLAGS) $< -o $@


.PHONY : clean
clean :
	rm -f *.o libgcore.so
