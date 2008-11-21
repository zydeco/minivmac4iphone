#import "vMacApp.h"
#import "MainView.h"

vMacApp* _vmacAppSharedInstance = nil;

GLOBALPROC notifyDiskEjected(ui4b Drive_No);
GLOBALPROC notifyDiskInserted(ui4b Drive_No, blnr locked);
GLOBALFUNC blnr getFirstFreeDisk(ui4b *Drive_No);
IMPORTFUNC blnr InitEmulation(void);

@implementation vMacApp

+ (id)sharedInstance {
    return _vmacAppSharedInstance;
}

- (void)applicationDidFinishLaunching:(UIApplication *)application {
    _vmacAppSharedInstance = self;
    
    // initialize stuff
    openAlerts = [[NSMutableSet setWithCapacity:5] retain];
    searchPaths = [[NSArray arrayWithObjects:
                    [[NSBundle mainBundle] resourcePath],
                    [NSHomeDirectory() stringByAppendingPathComponent:@"Library/MacOSClassic"],
                    @"/Library/MacOSClassic",
                    nil] retain];
    initOk = [self initEmulation];
    
    // initialize defaults
    [self initPreferences];
    
    // create window
    window = [[UIWindow alloc] initWithFrame: CGRectMake(0,0,480,320)];
    [window setTransform:CGAffineTransformMake(0, 1, -1, 0, -80, 80)];
    mainView = [[MainView alloc] initWithFrame: CGRectMake(0,0,480,320)];
    [window setContentView:mainView];
    [window orderFront:self];
    [window makeKey:self];
    
    // start emulation
    if (initOk) [self startEmulation:self];
}

- (void)dealloc {
    [window release];
    [openAlerts release];
    [romData release];
    [searchPaths release];
    [super dealloc];
}

- (void)applicationSuspend:(GSEvent *)event {
    if (self.insertedDisks == 0)
        [self terminate];
    else
        [self suspendEmulation];
}


- (void)applicationDidResume {
    [self resumeEmulation];
}

- (void)initPreferences {
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
    if ([defaults stringForKey:@"KeyboardLayout"] == nil)
        [defaults setObject:@"US" forKey:@"KeyboardLayout"];
    if ([defaults objectForKey:@"ScreenSizeToFit"] == nil)
        [defaults setBool:YES forKey:@"ScreenSizeToFit"];
    if ([defaults objectForKey:@"KeyboardAlpha"] == nil)
        [defaults setFloat:0.8 forKey:@"KeyboardAlpha"];
    if ([defaults objectForKey:@"ScreenPosition"] == nil)
        [defaults setInteger:dirUp|dirLeft forKey:@"ScreenPosition"];
    [defaults synchronize];
}

- (NSArray*)searchPaths {
    return searchPaths;
}

#if 0
#pragma mark -
#pragma mark Mouse
#endif

- (void)setMouseButtonDown {
    CurMouseButton = YES;
}

- (void)setMouseButtonUp {
    CurMouseButton = NO;
}

- (void)setMouseButton:(BOOL)pressed {
    CurMouseButton = pressed;
}

- (void)setMouseLoc:(Point)mouseLoc {
    CurMouseH = mouseLoc.h;
    CurMouseV = mouseLoc.v;
}

- (void)setMouseLoc:(Point)mouseLoc button:(BOOL)pressed {
    CurMouseH = mouseLoc.h;
    CurMouseV = mouseLoc.v;
    CurMouseButton = pressed;
}

#if 0
#pragma mark -
#pragma mark Keyboard
#endif

- (void)vKeyDown:(int)key {
    ui3b *kp = (ui3b *)theKeys;
    
    if (key >= 0 && key < 128) {
        int bit = 1 << (key & 7);
        kp[key / 8] |= bit;
    }
}

- (void)vKeyUp:(int)key {
    ui3b *kp = (ui3b *)theKeys;
    
    if (key >= 0 && key < 128) {
        int bit = 1 << (key & 7);
        kp[key / 8] &= ~ bit;
    }
}

- (NSDictionary*)availableKeyboardLayouts {
    NSMutableDictionary* layouts = [NSMutableDictionary dictionaryWithCapacity:5];
    NSFileManager* fm = [NSFileManager defaultManager];
    NSArray* sources = [self searchPaths];
    NSArray* extensions = [NSArray arrayWithObject:@"kbdlayout"];
    
    for(NSString* dir in sources) {
        NSArray* files = [[fm contentsOfDirectoryAtPath:dir error:NULL] pathsMatchingExtensions:extensions];
        for(NSString* file in files) {
            NSString* layoutID = [file stringByDeletingPathExtension];
            NSDictionary* kbFile = [NSDictionary dictionaryWithContentsOfFile:[dir stringByAppendingPathComponent:file]];
            NSString* layoutName = [kbFile objectForKey:@"Name"];
            [layouts setObject:layoutName forKey:layoutID];
        }
    }
    
    return [NSDictionary dictionaryWithDictionary:layouts];
}

#if 0
#pragma mark -
#pragma mark Disk
#endif

- (BOOL)initDrives {
    // initialize drives
    int i;
    numInsertedDisks = 0;
    for(i=0; i < NumDrives; i++) {
        drives[i] = nil;
        drivePath[i] = nil;
    }
    return YES;
}

- (BOOL)diskIsInserted:(NSString*)path {
    for(int i = 0; i < NumDrives; i++)
        if ([drivePath[i] isEqualToString:path]) return YES;
    return NO;
}

- (NSInteger) insertedDisks {
    return numInsertedDisks;
}

- (BOOL)insertDisk:(NSString*)path {
    BOOL isDir;
    short i, driveNum;
    NSFileManager*  mgr = [NSFileManager defaultManager];
    // check for free drive
    if (!getFirstFreeDisk(&driveNum)) return NO;
    // check for file
    if ([mgr fileExistsAtPath:path isDirectory:&isDir] == NO) return NO;
    if (isDir) return NO;
    // check if disk is inserted
    if ([self diskIsInserted:path]) return NO;
    // insert disk
    if ([mgr isWritableFileAtPath:path]) {
        drives[driveNum] = [[NSFileHandle fileHandleForUpdatingAtPath:path] retain];
        notifyDiskInserted(driveNum, falseblnr);
    } else {
        drives[driveNum] = [[NSFileHandle fileHandleForReadingAtPath:path] retain];
        notifyDiskInserted(driveNum, trueblnr);
    }
    drivePath[driveNum] = [path retain];
    numInsertedDisks++;
    return YES; 
}

- (short)readFromDrive:(short)n start:(unsigned long)start count:(unsigned long*)count buffer:(void*)buffer {
    [drives[n] seekToFileOffset:(unsigned long long)start];
    read([drives[n] fileDescriptor], buffer, (size_t)*count);
    return 0;
}

- (short)writeToDrive:(short)n start:(unsigned long)start count:(unsigned long*)count buffer:(void*)buffer {
    NSData *data = [NSData dataWithBytesNoCopy:buffer length:(NSUInteger)*count];
    [drives[n] seekToFileOffset:(unsigned long long)start];
    [drives[n] writeData:data];
    return 0;
}
- (short)sizeOfDrive:(short)n count:(unsigned long*)count {
    unsigned long long size = [drives[n] seekToEndOfFile];
    *count = (ui5b)size;
    return 0;
}

- (short)ejectDrive:(short)n {
    NSFileHandle    *fh = drives[n];
    drives[n] = nil;
    [drivePath[n] release];
    drivePath[n] = nil;
    numInsertedDisks--;
    
    notifyDiskEjected(n);
    
    [fh closeFile];
    [fh release];
    
    return 0;
}

#if 0
#pragma mark -
#pragma mark Alerts
#endif

- (void)warnMessage:(NSString *)message {
    UIAlertView* alert = [[UIAlertView alloc] initWithTitle:@"Warning" message:message delegate:self cancelButtonTitle:@"OK" otherButtonTitles:nil];
    [openAlerts addObject:alert];
    SpeedStopped = trueblnr;
    [alert show];
}

- (void)alertView:(UIAlertView *)alertView didDismissWithButtonIndex:(NSInteger)buttonIndex {
    [openAlerts removeObject:alertView];
    if (([openAlerts count] == 0) && initOk) SpeedStopped = falseblnr;
}

#if 0
#pragma mark -
#pragma mark Misc
#endif

- (BOOL)loadROM {
    // find ROM
    NSString*   romPath = nil;
    NSString*   romFileName = [NSString stringWithUTF8String:RomFileName];
    NSFileManager* fm = [NSFileManager defaultManager];
    NSArray*    romSearchPaths = [self searchPaths];
    
    for(NSString* p in romSearchPaths) {
        romPath = [p stringByAppendingPathComponent:romFileName];
        if ([fm isReadableFileAtPath:romPath]) break;
    }
    
    // read ROM from first found file
    romData = [NSData dataWithContentsOfFile:romPath];
    if (romData == nil) {
        [self warnMessage:@"Could not load ROM file"];
        return NO;
    }
    ROM = (ui4b*)[romData bytes];
    [romData retain];
    return YES;
}

#if 0
#pragma mark -
#pragma mark Emulation
#endif
- (BOOL)initEmulation {
    // load ROM
    if (![self loadROM]) return NO;
    
    // allocate RAM
    RAM = (ui4b*)calloc(1, kRAM_Size + RAMSafetyMarginFudge);
    if (RAM == NULL) {
        [self warnMessage:@"Could not allocate RAM"];
        return NO;
    }
    
    // allocate screen
    screencomparebuff = malloc(vMacScreenNumBytes);
    if (screencomparebuff == NULL) {
        [self warnMessage:@"Could not allocate screen buffer"];
        return NO;
    }
    memset(screencomparebuff, 0xFF, vMacScreenNumBytes);
    
    // init location
    NSTimeZone *ntz = [NSTimeZone localTimeZone];
    // I'm not really going to use the location services
    CurMacLatitude = 0;
    CurMacLongitude = 0;
    CurMacDelta = [ntz secondsFromGMT]/3600;
    CFTimeZoneRef tz = CFTimeZoneCopySystem();
    aTimeBase = CFAbsoluteTimeGetCurrent();
    CFGregorianDate d = CFAbsoluteTimeGetGregorianDate(aTimeBase, tz);
    double floorsec = floor(d.second);
    aTimeBase -= (d.second - floorsec);
    timeSecBase = Date2MacSeconds(floorsec, d.minute, d.hour, d.day, d.month, d.year);
    CFRelease(tz);
    
    // init drives
    if (![self initDrives]) {
        [self warnMessage:@"Could not initialize drives"];
        return NO;
    }
    
    // init emulation
    if (!InitEmulation()) {
        [self warnMessage:@"Could not initialize emulation"];
        return NO;
    }
    return YES;
}

- (void)startEmulation:(id)sender {
    // load initial disk images
    NSFileManager* mng = [NSFileManager defaultManager];
    for(int i=1; i <= NumDrives; i++) {
        NSString *path = [[NSBundle mainBundle] pathForResource:[NSString stringWithFormat:@"disk%d", i] ofType:@"dsk"];
        if ([mng fileExistsAtPath:path]) [self insertDisk:path];
    }
    
    #if MySoundEnabled
        MySound_Start();
    #endif
    
    [self resumeEmulation];
}

- (void)resumeEmulation {
    // create and start emulation tick timer
    CFRunLoopTimerContext tCtx = {0, NULL, NULL, NULL, NULL};
    tickTimer = CFRunLoopTimerCreate(kCFAllocatorDefault, 0, MyTickDuration, 0, 0, runTick, &tCtx);
    CFRunLoopAddTimer(CFRunLoopGetMain(), tickTimer, kCFRunLoopCommonModes);
    
    // set speed
    SpeedStopped = falseblnr;
}

- (void)suspendEmulation {
    SpeedStopped = trueblnr;
    CFRunLoopRemoveTimer(CFRunLoopGetMain(), tickTimer, kCFRunLoopCommonModes);
}

@end
