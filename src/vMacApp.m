#import "vMacApp.h"
#import "MainView.h"
#import <Foundation/NSTask.h>
#import "ExtendedAttributes.h"
#include <sys/param.h>
#include <sys/mount.h>

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
    [self setStatusBarOrientation:UIInterfaceOrientationLandscapeRight animated:NO];
    NSFileManager * fm = [NSFileManager defaultManager];
    NSBundle * mb = [NSBundle mainBundle];
    openAlerts = [[NSMutableSet setWithCapacity:5] retain];
    searchPaths = [[NSArray arrayWithObjects:
                    [mb resourcePath],
                    [NSHomeDirectory() stringByAppendingPathComponent:@"Library/MacOSClassic"],
                    @"/Library/MacOSClassic",
                    nil] retain];
    initOk = [self initEmulation];
    AudioServicesCreateSystemSoundID((CFURLRef)[NSURL fileURLWithPath:[mb pathForResource:@"diskEject" ofType:@"aiff"]] ,&ejectSound);
    
    // check default search path
    if (![fm fileExistsAtPath:[self defaultSearchPath]])
        [fm createDirectoryAtPath:[self defaultSearchPath] attributes:nil];
    
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
    
    // create disk icons
    [self performSelectorInBackground:@selector(createDiskIcons:) withObject:[NSNumber numberWithBool:YES]];
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [window release];
    [openAlerts release];
    [romData release];
    [searchPaths release];
    AudioServicesDisposeSystemSoundID(ejectSound);
    [super dealloc];
}

- (void)applicationSuspend:(GSEventRef)event {
    if (self.insertedDisks == 0)
        exit(0);
    else
        [self suspendEmulation];
}


- (void)applicationDidResume {
    [self resumeEmulation];
}

- (NSArray*)searchPaths {
    return searchPaths;
}

- (NSString*)defaultSearchPath {
    [NSHomeDirectory() stringByAppendingPathComponent:@"Library/MacOSClassic"];
}

#if 0
#pragma mark -
#pragma mark Preferences
#endif

- (void)initPreferences {
    defaults = [NSUserDefaults standardUserDefaults];
    if ([defaults stringForKey:@"KeyboardLayout"] == nil)
        [defaults setObject:@"US" forKey:@"KeyboardLayout"];
    if ([defaults objectForKey:@"ScreenSizeToFit"] == nil)
        [defaults setBool:YES forKey:@"ScreenSizeToFit"];
    if ([defaults objectForKey:@"KeyboardAlpha"] == nil)
        [defaults setFloat:0.8 forKey:@"KeyboardAlpha"];
    if ([defaults objectForKey:@"ScreenPosition"] == nil)
        [defaults setInteger:dirUp|dirLeft forKey:@"ScreenPosition"];
    if ([defaults objectForKey:@"SoundEnabled"] == nil)
        [defaults setBool:YES forKey:@"SoundEnabled"];
    if ([defaults objectForKey:@"DiskEjectSound"] == nil)
        [defaults setBool:YES forKey:@"DiskEjectSound"];
    if ([defaults objectForKey:@"TrackpadMode"] == nil)
        [defaults setBool:NO forKey:@"TrackpadMode"];
    if ([defaults objectForKey:@"KeyboardSound"] == nil)
        [defaults setBool:YES forKey:@"KeyboardSound"];
    if ([defaults objectForKey:@"CanDeleteDiskImages"] == nil)
        [defaults setBool:YES forKey:@"CanDeleteDiskImages"];
    
    [defaults synchronize];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didChangePreferences:) name:@"preferencesUpdated" object:nil];
}

- (void)didChangePreferences:(NSNotification *)aNotification {
    if ([defaults boolForKey:@"SoundEnabled"]) MySound_Start();
    else MySound_Stop();
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
    HaveMouseMotion = falseblnr;
    CurMouseH = CLAMP(mouseLoc.h, 0, vMacScreenWidth);
    CurMouseV = CLAMP(mouseLoc.v, 0, vMacScreenHeight);
}

- (void)setMouseLoc:(Point)mouseLoc button:(BOOL)pressed {
    HaveMouseMotion = falseblnr;
    CurMouseH = CLAMP(mouseLoc.h, 0, vMacScreenWidth);
    CurMouseV = CLAMP(mouseLoc.v, 0, vMacScreenHeight);
    CurMouseButton = pressed;
}

- (void)moveMouse:(Point)mouseMotion {
    HaveMouseMotion = trueblnr;
    MouseMotionH = mouseMotion.h;
    MouseMotionV = mouseMotion.v;
}

- (void)moveMouse:(Point)mouseMotion button:(BOOL)pressed {
    HaveMouseMotion = trueblnr;
    MouseMotionH = mouseMotion.h;
    MouseMotionV = mouseMotion.v;
    CurMouseButton = pressed;
}

- (Point)mouseLoc {
    Point pt;
    pt.h = CurMouseH;
    pt.v = CurMouseV;
    return pt;
}

- (BOOL)mouseButton {
    return CurMouseButton;
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
            id layoutName = [kbFile objectForKey:@"Name"];
            if ([layoutName isKindOfClass:[NSDictionary class]]) {
                NSString* localization = [[[NSBundle mainBundle] preferredLocalizations] objectAtIndex:0];
                NSString* localizedLayoutName = [layoutName objectForKey:localization];
                if (localizedLayoutName == nil) localizedLayoutName = [layoutName objectForKey:@"English"];
                [layouts setObject:localizedLayoutName forKey:layoutID];
            } else
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

- (NSInteger)insertedDisks {
    return numInsertedDisks;
}

- (BOOL)canCreateDiskImages {
    NSLog(@"I can%s create disk images", [[NSFileManager defaultManager] isWritableFileAtPath:self.pathToDiskImages]?"":"'t");
    return [[NSFileManager defaultManager] isWritableFileAtPath:self.pathToDiskImages];
}

- (NSString*)pathToDiskImages {
    return [self defaultSearchPath];
}

- (BOOL)insertDisk:(NSString*)path {
    BOOL isDir;
    short i, driveNum;
    NSFileManager*  mgr = [NSFileManager defaultManager];
    // check for free drive
    if (!getFirstFreeDisk(&driveNum)) {
        [self warnMessage:NSLocalizedString(@"TooManyDisksText", nil) title:NSLocalizedString(@"TooManyDisksTitle", nil)];
        return NO;
    }
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
    
    [[NSNotificationCenter defaultCenter] postNotificationName:@"diskInserted" object:self];
    
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

- (BOOL)ejectDrive:(short)n {
    NSFileHandle *fh = drives[n];
    drives[n] = nil;
    [drivePath[n] release];
    drivePath[n] = nil;
    numInsertedDisks--;
    
    notifyDiskEjected(n);
    
    [fh closeFile];
    [fh release];
    
    if ([defaults boolForKey:@"DiskEjectSound"]) AudioServicesPlaySystemSound(ejectSound);
    [[NSNotificationCenter defaultCenter] postNotificationName:@"diskEjected" object:self];
    
    return YES;
}

- (NSArray*)availableDiskImages {
    NSMutableArray* myDiskFiles = [NSMutableArray arrayWithCapacity:10];
    NSFileManager* fm = [NSFileManager defaultManager];
    NSArray* sources = [self searchPaths];
    NSArray* extensions = [NSArray arrayWithObjects: @"dsk", @"img", @"DSK", @"IMG", @"image", nil];
    
    for(NSString *srcDir in sources) {
        NSArray *dirFiles = [[fm contentsOfDirectoryAtPath:srcDir error:NULL] pathsMatchingExtensions:extensions];
        for(NSString *filename in dirFiles)
            [myDiskFiles addObject:[srcDir stringByAppendingPathComponent:filename]];
    }
    
    return myDiskFiles;
}

- (void)createDiskIcons:(NSNumber*)force {
    if ([NSThread isMainThread]) {
        [self performSelectorInBackground:@selector(createDiskIcons:) withObject:force];
        return;
    }
    
    NSAutoreleasePool * pool;
    NSMutableArray * taskArgs;
    NSArray * diskImages;
    
    pool = [NSAutoreleasePool new];
    
    // find disks
    taskArgs = [NSMutableArray arrayWithObject:@"-x:net.namedfork.DiskImageIcon"];
    diskImages = [self availableDiskImages];
    
    // check which disk images need icons
    if ((force == nil) || ([force boolValue] == NO)) {
        for(NSString * diskImage in diskImages)
            if ([self diskImageHasIcon:diskImage] == NO) [taskArgs addObject:diskImage];
    }
    
    // return if no disk images selected
    if ([taskArgs count] == 1) {
        [pool release];
        return;
    }
    
    // create task
    NSTask * task = [[NSTask alloc] init];
    [task setLaunchPath:[[NSBundle mainBundle] pathForAuxiliaryExecutable:@"dskicon"]];
    [task setArguments:taskArgs];
    [task launch];
    [task waitUntilExit];
    if ([task terminationStatus]) {
        NSLog(@"Created %d icons", [task terminationStatus]);
        [[NSNotificationCenter defaultCenter] postNotificationName:@"diskIconUpdate" object:nil];
    }
    
    [task release];
    [pool release];
}

- (BOOL)diskImageHasIcon:(NSString*)path {
    NSFileManager* fm = [NSFileManager defaultManager];
    
    // xattr
    if ([fm hasExtendedAttribute:@"net.namedfork.DiskImageIcon" atPath:path traverseLink:YES error:NULL]) return YES;
    
    // file
    if ([fm fileExistsAtPath:[[path stringByDeletingPathExtension] stringByAppendingPathExtension:@"png"]]) return YES;
    
    return NO;
}

- (BOOL)createDiskImage:(NSString*)name size:(int)size {
    // size is in KiB
    if (name == nil || [name length] == 0) return NO;
    if (size < 400) return NO;
    if (size > 500 * 1024) return NO;
    
    // see if we have enough free space
    uint64_t freeSpace;
    uint64_t needSpace;
    struct statfs fss;
    if (statfs([[self pathToDiskImages] fileSystemRepresentation], &fss)) return NO;
    freeSpace = fss.f_bavail; // free blocks
    freeSpace *= fss.f_bsize; // now in bytes
    freeSpace /= 1024; // now in KiB
    needSpace = size + (20*1024); // 20 MiB safety margin
    if (freeSpace < needSpace) {
        [self warnMessage:NSLocalizedString(@"NotEnoughSpaceMsg", nil)];
        return NO;
    }
    
    // check filename
    NSFileManager * fm = [NSFileManager defaultManager];
    NSString * imagePath = [[[self pathToDiskImages] stringByAppendingPathComponent:name] stringByAppendingPathExtension:@"dsk"];
    if ([fm fileExistsAtPath:imagePath]) {
        [self warnMessage:NSLocalizedString(@"ImageAlreadyExistsMsg", nil)];
        return NO;
    }
    
    // write file
    FILE * fp = fopen([imagePath fileSystemRepresentation], "w");
    if (fp == NULL) {
        [self warnMessage:NSLocalizedString(@"ImageCreationError", nil)];
        return NO;
    }
    newImageFile = fp;
    newImageSize = size;
    if (size > 5*1024) {
        newImageProgress = [[UIModalView alloc] init];
        [newImageProgress setTitle:NSLocalizedString(@"CreatingDiskImage", nil)];
        [newImageProgress setBodyText:NSLocalizedString(@"CreatingDiskImageWait", nil)];
        [newImageProgress popupAlertAnimated:YES];
        [self performSelectorInBackground:@selector(writeDiskImageThread) withObject:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(writeDiskImageThreadDone:) name:@"diskCreated" object:nil];
    } else {
        newImageProgress = nil;
        [self writeDiskImageThread];
    }
    
    return YES;
}

- (void)writeDiskImageThread {
    int wbsize = 1024*100; // write in 100K blocks
    int wbytes = newImageSize*1024;
    if (newImageSize > 2048) wbsize = 1024*1024; // write in 1M blocks
    char * buf = malloc(wbsize);
    while(wbytes) {
        if (wbytes < wbsize) wbsize = wbytes;
        if (fwrite(buf, wbsize, 1, newImageFile) != 1) break;
        wbytes -= wbsize;
    }
    
    free(buf);
    fclose(newImageFile);
    newImageFile = NULL;
    
    [[NSNotificationCenter defaultCenter] postNotificationName:@"diskCreated" object:[NSNumber numberWithBool:(wbytes? NO : YES)]];
}

- (void)writeDiskImageThreadDone:(NSNotification*)notification {
    BOOL success = [[notification object] boolValue];
    [newImageProgress dismissAnimated:YES];
    [newImageProgress release];
    newImageProgress = nil;
    if (!success) [self warnMessage:NSLocalizedString(@"ImageCreationError", nil)];
}

#ifdef IncludeSonyGetName
- (NSString*)nameOfDrive:(short)n {
    if (drivePath[n] == nil) return nil;
    return [drivePath[n] lastPathComponent];
}
#endif

#ifdef IncludeSonyNew
- (BOOL)ejectAndDeleteDrive:(short)n {
    BOOL hadDiskEjectSound = [defaults boolForKey:@"DiskEjectSound"];
    BOOL success = NO;
    [defaults setBool:NO forKey:@"DiskEjectSound"];
    NSString * path = [drivePath[n] autorelease];
    if ([self ejectDrive:n])
        success = [[NSFileManager defaultManager] removeItemAtPath:path error:NULL];
    [defaults setBool:hadDiskEjectSound forKey:@"DiskEjectSound"];
    return success;
}
#endif

#if 0
#pragma mark -
#pragma mark Alerts
#endif

- (void)warnMessage:(NSString *)message title:(NSString *)title {
    UIAlertView* alert = [[UIAlertView alloc] initWithTitle:title message:message delegate:self cancelButtonTitle:NSLocalizedString(@"OK", nil) otherButtonTitles:nil];
    [openAlerts addObject:alert];
    SpeedStopped = trueblnr;
    [alert show];
}

- (void)warnMessage:(NSString *)message {
    [self warnMessage:message title:NSLocalizedString(@"WarnTitle", nil)];
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
        [self warnMessage:[NSString stringWithFormat:NSLocalizedString(@"WarnNoROM", nil), RomFileName]];
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
        [self warnMessage:NSLocalizedString(@"WarnNoRAM", nil)];
        return NO;
    }
    
    // allocate screen
    screencomparebuff = malloc(vMacScreenNumBytes);
    if (screencomparebuff == NULL) {
        [self warnMessage:NSLocalizedString(@"WarnNoScreen", nil)];
        return NO;
    }
    memset(screencomparebuff, 0xFF, vMacScreenNumBytes);
    
    // pixel conversion table
    pixelConversionTable = malloc(sizeof(short)*8*256);
    for(int i=0; i < 256; i++) {
        for(int j=0; j < 8; j++) pixelConversionTable[8*i+j] = ((i & (0x80 >> j)) ? 0x0000 : 0xFFFF);
    }
    
    // init location
    NSTimeZone *ntz = [NSTimeZone localTimeZone];
    CurMacDelta = [ntz secondsFromGMT]/3600;
    MacDateDiff = kMacEpoch + [ntz secondsFromGMT];
    CurMacDateInSeconds = time(NULL) + MacDateDiff;
    
    // init drives
    if (![self initDrives]) {
        [self warnMessage:NSLocalizedString(@"WarnNoDrives", nil)];
        return NO;
    }
    
    // init sound
    #if MySoundEnabled
    if (!MySound_Init()) [self warnMessage:NSLocalizedString(@"WarnNoSound", nil)];
    #endif
    
    // init emulation
    if (!InitEmulation()) {
        [self warnMessage:NSLocalizedString(@"WarnNoEmu", nil)];
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
    
    [self resumeEmulation];
}

- (void)resumeEmulation {
    // set speed
    StartUpTimeAdjust();
    SpeedStopped = falseblnr;
    
    // create and start emulation tick timer
    CFRunLoopTimerContext tCtx = {0, NULL, NULL, NULL, NULL};
    tickTimer = CFRunLoopTimerCreate(kCFAllocatorDefault, 0, MyTickDuration, 0, 0, runTick, &tCtx);
    CFRunLoopAddTimer(CFRunLoopGetMain(), tickTimer, kCFRunLoopCommonModes);
    
    #if MySoundEnabled
        if ([defaults boolForKey:@"SoundEnabled"]) MySound_Start();
    #endif
}

- (void)suspendEmulation {
    #if MySoundEnabled
        MySound_Stop();
    #endif
    
    SpeedStopped = trueblnr;
    CFRunLoopRemoveTimer(CFRunLoopGetMain(), tickTimer, kCFRunLoopCommonModes);
}

@end
