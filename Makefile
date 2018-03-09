CC = ${CROSS_COMPILE}gcc
INCLUDES := -I. -I./board -I../driver -I./lib/jsmn -I./lib/avl -I./lib/progress
CFLAGS := ${DRIVER_CFLAGS} -c -Wall -Werror -fpic -D_FILE_OFFSET_BITS=64

LDFLAGS :=
EXECUTABLE :=
OS := $(shell uname)
ifeq ($(OS),Darwin)
	LDFLAGS += -shared -Wl,-install_name,libgcore.dylib 
	EXECUTABLE += libgcore.dylib
else
	LDFLAGS += -shared -Wl,-soname,libgcore.so 
	EXECUTABLE += libgcore.so
endif

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
	rm -f $(OBJS) $(EXECUTABLE)
