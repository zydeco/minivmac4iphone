LIB = libres.a

CC = gcc
AR = ar
CFLAGS = -g -std=c99

all: $(LIB)

$(LIB): res.c
	$(CC) -c $(CFLAGS) res.c
	$(AR) -ru $(LIB) res.o

clean:
	rm -rf $(LIB) res.o demo