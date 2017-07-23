CC = ${CROSS_COMPILE}gcc
LDFLAGS := -lgcore
CFLAGS := ${DRIVER_CFLAGS} -c -Wall -Werror
INCLUDES := -I. -I../driver 


.PHONY : all
all : libgcore


libgcore.o : libgcore.c libgcore.h
	$(CC) $(CFLAGS) $(INCLUDES) -fpic libgcore.c


libgcore : libgcore.o
	$(CC) -shared -Wl,-soname,libgcore.so -o libgcore.so libgcore.o


#install : libgcore
#	sudo cp ${PWD}/libgcore.so /usr/lib
#	sudo cp ${PWD}/libgcore.h /usr/include
#	sudo chmod 0755 /usr/lib/libgcore.so
#	sudo ldconfig


#uninstall :
#	sudo rm /usr/lib/libgcore.so
#	sudo rm /usr/include/libgcore.h
#	sudo ldconfig


.PHONY : clean
clean :
	rm -f *.o libgcore.so
