PROD = minivmac
APP  = minivmac.app
VERSION=1.2

MNVM = ADDRSPAC.o \
       GLOBGLUE.o \
       IWMEMDEV.o \
       KBRDEMDV.o \
       MINEM68K.o \
       MOUSEMDV.o \
       PROGMAIN.o \
       ROMEMDEV.o \
       RTCEMDEV.o \
       SCCEMDEV.o \
       SCRNEMDV.o \
       SCSIEMDV.o \
       SNDEMDEV.o \
       SONYEMDV.o \
       VIAEMDEV.o
OBJS = $(MNVM) main.o glue.o vMacApp.o MainView.o \
       SurfaceView.o ExtendedAttributes.o KeyboardView.o KBKey.o \
       InsertDiskView.o NewDiskView.o SettingsView.o

EXTRAS = Resources/dskicon

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
         -march=armv6 -mcpu=arm1176jzf-s -fomit-frame-pointer -O3 \
         -include src/prefix.h -Isrc/mnvm -DVERSION="$(VERSION)"

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

app: build/$(APP)

build/$(APP): $(PROD) $(EXTRAS)
	mkdir -p build/$(APP)
	cp -r Resources/* build/$(APP)/
	cp $(PROD) build/$(APP)/
	sed s/BUNDLE_VERSION/$(VERSION)/ Resources/Info.plist > build/$(APP)/Info.plist
	plutil -convert binary1 build/$(APP)/*.kbdlayout
	plutil -convert binary1 build/$(APP)/*.lproj/Localizable.strings
	rm -rf build/$(APP)/.svn build/$(APP)/*/.svn build/$(APP)/.DS_Store build/$(APP)/*/.DS_Store

clean:
	rm -rf $(OBJS) $(PROD) $(EXTRAS)
	rm -rf ./build

clean-all: clean
	make -eC dskicon -f Makefile.iPhone clean-all

Resources/dskicon: dskicon/dskicon
	cp dskicon/dskicon Resources/dskicon

dskicon/dskicon: $(DSKICON_FILES)
	make -eC dskicon -f Makefile.iPhone
	
install: app
	scp -r build/$(APP) root@$(IPHONE):/Applications
	ssh $(IPHONE) -l mobile uicache

reinstall: app
	ssh $(IPHONE) rm -f /Applications/$(APP)/$(PROD)
	scp -r build/$(APP)/$(PROD) root@$(IPHONE):/Applications/$(APP)/$(PROD)

reinstall-all: app
	ssh $(IPHONE) rm -f /Applications/$(APP)/$(PROD) /Applications/$(APP)/dskicon
	scp -r build/$(APP) root@$(IPHONE):/Applications

dist: app
	mkdir -p build/$(PROD)/{Applications,DEBIAN}
	cp -r build/$(APP) build/$(PROD)/Applications/
	rm -f build/$(PROD)/Applications/$(APP)/*.{dsk,img,rom,ROM}
	sed 's/\$$VERSION/$(VERSION)/g' apt-control > build/$(PROD)/DEBIAN/control
	echo Installed-Size: `du -ck build/$(PROD) | tail -1 | cut -f 1` >> build/$(PROD)/DEBIAN/control
	COPYFILE_DISABLE="" COPY_EXTENDED_ATTRIBUTES_DISABLE="" dpkg-deb -b build/$(PROD)
