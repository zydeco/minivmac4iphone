LIB = libres.a

CC = gcc
AR = ar
CFLAGS = -g -std=c99

all: $(LIB)

$(LIB): libres.c
	$(CC) -c $(CFLAGS) libres.c
	$(AR) -ru $(LIB) libres.o

clean:
	rm -rf $(LIB) libres.o demo