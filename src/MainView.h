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
    NSTimeInterval  lastMouseTime;
    Point           lastMouseLoc;
    Point           mouseOffset;
    // gesture
    CGPoint         gestureStart;
}

- (Point)mouseLocForEvent:(GSEvent *)event;

- (void)toggleScreenSize;
- (void)scrollScreenViewTo:(Direction)scroll;

- (void)twoFingerSwipeGesture:(Direction)direction;
- (void)twoFingerTapGesture:(GSEvent *)event;

@end