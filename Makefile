PROD = minivmac
APP  = minivmac.app
VERSION=1.0.2

MNVM = ADDRSPAC.o \
       GLOBGLUE.o \
       IWMEMDEV.o \
       KBRDEMDV.o \
       MINEM68K.o \
       MOUSEMDV.o \
       MYOSGLUE.o \
       PROGMAIN.o \
       ROMEMDEV.o \
       RTCEMDEV.o \
       SCCEMDEV.o \
       SCRNEMDV.o \
       SCSIEMDV.o \
       SNDEMDEV.o \
       SONYEMDV.o \
       VIAEMDEV.o
OBJS = $(MNVM) main.o glue.o \
       vMacApp.o MainView.o SurfaceView.o \
       SettingsView.o InsertDiskView.o \
       KeyboardView.o KBKey.o

CC = arm-apple-darwin9-gcc
LD = $(CC)
LDID = arm-apple-darwin9-ldid
MD5 = md5
IPHONE = iphone

LDFLAGS = -framework Foundation \
          -framework CoreFoundation \
          -framework UIKit \
          -framework QuartzCore \
          -framework CoreGraphics \
          -framework GraphicsServices \
          -framework CoreSurface \
          -framework CoreAudio \
          -framework Celestial \
          -framework AudioToolbox \
          -framework IOKit \
          -F/System/Library/PrivateFrameworks \
          -lobjc \
          -bind_at_load \
          -multiply_defined suppress

CFLAGS = -Werror -std=c99 \
         -march=armv6 -mcpu=arm1176jzf-s -fomit-frame-pointer -O2 \
         -Isrc/mnvm -DVERSION="$(VERSION)"

all: $(PROD) app
	
$(PROD): $(OBJS)
	$(LD) $(CFLAGS) $(LDFLAGS) -o $(PROD) $^
	$(LDID) -S $(PROD)

%.o:	src/%.m
	$(CC) -c $(CFLAGS) $< -o $@

%.o:	src/%.c
	$(CC) -c $(CFLAGS) $< -o $@

%.o:	src/mnvm/%.c
	$(CC) -c $(CFLAGS) $< -o $@

app: $(PROD)
	rm -rf build
	mkdir build
	mkdir build/$(APP)
	cp -r Resources/* build/$(APP)/
	cp $(PROD) build/$(APP)/
	sed s/BUNDLE_VERSION/$(VERSION)/ Resources/Info.plist > build/$(APP)/Info.plist
	plutil -convert binary1 build/$(APP)/*.kbdlayout

clean:
	rm -rf $(OBJS) $(PROD)
	rm -rf ./build

install: app
	scp -r build/$(APP) root@$(IPHONE):/Applications
	ssh $(IPHONE) respring

dist: app
	mkdir -p build/$(PROD)/{Applications,DEBIAN}
	cp -r build/$(APP) build/$(PROD)/Applications/
	rm -f build/$(PROD)/Applications/$(APP)/*.{dsk,img,rom,ROM}
	sed 's/\$$VERSION/$(VERSION)/g' apt-control > build/$(PROD)/DEBIAN/control
	echo Installed-Size: `du -ck build/$(PROD) | tail -1 | cut -f 1` >> build/$(PROD)/DEBIAN/control
	COPYFILE_DISABLE="" COPY_EXTENDED_ATTRIBUTES_DISABLE="" dpkg-deb -b build/$(PROD)
