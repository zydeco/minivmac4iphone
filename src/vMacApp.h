#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <UIKit/UIAlertView.h>
#import "KeyboardView.h"

#import "SYSDEPNS.h"
#import "MYOSGLUE.h"
#import "PROGMAIN.h"

#define MyFrameSkip 3
#define MyTickDuration (1/60.14742)

#define PointDistanceSq(a, b) ((((int)a.h-(int)b.h)*((int)a.h-(int)b.h)) + (((int)a.v-(int)b.v)*((int)a.v-(int)b.v)))
#define CGPointCenter(a, b) CGPointMake((a.x+b.x)/2, (a.y+b.y)/2)
#define MOUSE_DBLCLICK_HELPER   1       // enable double-click assistance
#define MOUSE_DBLCLICK_TIME     0.7     // seconds, NSTimeInterval
#define MOUSE_CLICK_DELAY       0.05    // seconds, NSTimeInterval
#define MOUSE_LOC_THRESHOLD      100     // pixel distance in mac screen, squared, integer
#define kScreenEdgeSize         20      // edge size for scrolling
#define kScreenRectFullScreen   CGRectMake(0.f, 0.f, 480.f, 320.f)
#define kScreenRectRealSize     CGRectMake(0.f, 0.f, 512.f, 342.f)

#ifndef ABS
#define ABS(x) ((x>0)?(x):(-1*(x)))
#endif

typedef enum Direction {
    dirUp =     1 << 0,
    dirDown =   1 << 1,
    dirLeft =   1 << 2,
    dirRight =  1 << 3
} Direction;

@protocol VirtualDiskDrive
- (BOOL)diskIsInserted:(NSString*)path;
- (BOOL)insertDisk:(NSString*)path;
- (short)readFromDrive:(short)drive start:(unsigned long)start count:(unsigned long*)count buffer:(void*)buffer;
- (short)writeToDrive:(short)drive start:(unsigned long)start count:(unsigned long*)count buffer:(void*)buffer;
- (short)sizeOfDrive:(short)drive count:(unsigned long*)count;
- (short)ejectDrive:(short)drive;
@property (nonatomic, readonly) NSInteger insertedDisks;
@end

@protocol VirtualMouse
- (void)setMouseButton:(BOOL)pressed;
- (void)setMouseButtonDown;
- (void)setMouseButtonUp;
- (void)setMouseLoc:(Point)mouseLoc button:(BOOL)pressed;
- (void)setMouseLoc:(Point)mouseLoc;
@end

@class MainView;
@interface vMacApp : UIApplication <VirtualKeyboard, VirtualMouse, VirtualDiskDrive>
{
    UIWindow*   window;
    MainView*   mainView;
    BOOL        initOk;
    
    NSMutableSet*   openAlerts;
    NSFileHandle*   drives[NumDrives];
    NSString*       drivePath[NumDrives];
    NSData*         romData;
    
    CFRunLoopTimerRef   tickTimer;
    CFAbsoluteTime      aTimeBase;
    ui5b                timeSecBase;
}

+ (id)sharedInstance;

- (void)initPreferences;
- (void)warnMessage:(NSString *)message;
- (BOOL)initDrives;
- (BOOL)loadROM;
- (NSDictionary*)availableKeyboardLayouts;
- (void)startEmulation:(id)sender;
- (BOOL)initEmulation;
- (void)suspendEmulation;
- (void)resumeEmulation;
@end

extern vMacApp* _vmacAppSharedInstance;
extern NSInteger numInsertedDisks;
extern blnr SpeedStopped;
extern short* SurfaceScrnBuf;
extern id _gScreenView;
void runTick (CFRunLoopTimerRef timer, void* info);