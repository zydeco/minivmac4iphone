PROD=dskicon
OBJS=main.o DSKIconFactory.o

CC = gcc
LD = $(CC)
#TESTIMG = HyperCard.img
#TESTIMG = SampleMFS.dsk
TESTIMG = /Library/MacOSClassic/68k/macos76.img

# Mac OS X flags
ARCH_CFLAGS = -DBUILD_MACOSX
ARCH_LDFLAGS = -framework ApplicationServices -framework AppKit

LDFLAGS = -framework Foundation \
          -framework CoreFoundation \
          -F/System/Library/PrivateFrameworks \
          -lobjc -lmfs -lhfs -lres -lz \
          -bind_at_load -multiply_defined suppress \
          -Llibmfs -Llibres -Llibhfs \
          $(ARCH_LDFLAGS)


CFLAGS = -g -Werror -std=c99 -include prefix.h -fomit-frame-pointer -O2 \
         -I. $(ARCH_CFLAGS)

all: $(PROD)
	
$(PROD): $(OBJS) libres-lib libmfs-lib libhfs-lib
	$(LD) $(CFLAGS) $(LDFLAGS) -o $(PROD) $(OBJS)
#	$(LDID) -S $(PROD)

%.o:	%.m
	$(CC) -c $(CFLAGS) $< -o $@

%.o:	%.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -rf $(OBJS) $(PROD)

clean-all: clean
	make -eC libmfs clean
	make -eC libhfs clean
	make -eC libres clean

libmfs-lib:
	@make -eC libmfs

libhfs-lib:
	@make -eC libhfs

libres-lib:
	@make -eC libres

test: all
	@echo Testing...
	@./dskicon $(TESTIMG) out.png; true