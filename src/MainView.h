#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import "vMacApp.h"
#import "SurfaceView.h"
#import "InsertDiskView.h"
#import "SettingsView.h"

#define kSwipeThresholdHorizontal   100.0
#define kSwipeThresholdVertical     70.0

@interface MainView : UIView
{
    SurfaceView*    screenView;
    KeyboardView*   keyboardView;
    InsertDiskView* insertDiskView;
    SettingsView*   settingsView;
    
    // screen
    BOOL            screenSizeToFit;
    Direction       screenPosition;
    
    // mouse
    BOOL            trackpadMode, trackpadClick, trackpadDrag;
    BOOL            clickScheduled;
    Point           clickLoc;
    NSTimeInterval  lastMouseTime, lastMouseClick;
    Point           lastMouseLoc;
    Point           mouseOffset;
    // gesture
    CGPoint         gestureStart;
}

- (void)didChangePreferences:(NSNotification *)aNotification;

- (Point)mouseLocForEvent:(GSEventRef)event;
- (void)scheduleMouseClickAt:(Point)mouseLoc;
- (void)cancelMouseClick;
- (void)mouseClick;

- (void)toggleScreenSize;
- (void)scrollScreenViewTo:(Direction)scroll;

- (void)twoFingerSwipeGesture:(Direction)direction;
- (void)twoFingerTapGesture:(GSEventRef)event;

@end