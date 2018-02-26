CC = ${CROSS_COMPILE}gcc
INCLUDES := -I. -I../driver -I./lib/jsmn  -I./lib/avl -I./lib/progress -I../../tmp/ncurses-6.0/build/include
CFLAGS := ${DRIVER_CFLAGS} -c -Wall -Werror -fpic -D_FILE_OFFSET_BITS=64
LDFLAGS :=  -L../../tmp/ncurses-6.0/build/lib -shared -Wl,-soname,libgcore.so -Wl,-Bdynamic -lncurses 
EXECUTABLE = libgcore.so

SRCS = $(shell find . -name '*.c')
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
