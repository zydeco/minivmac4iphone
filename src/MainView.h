#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import "vMacApp.h"
#import "SurfaceView.h"
#import "InsertDiskView.h"
#import "SettingsView.h"

@interface MainView : UIView
{
    SurfaceView*    screenView;
    KeyboardView*   keyboardView;
    InsertDiskView* insertDiskView;
    SettingsView*   settingsView;
    
    // screen
    BOOL            screenSizeToFit;
    
    // mouse
    NSTimeInterval  lastMouseTime;
    Point           lastMouseLoc;
    Point           mouseOffset;
    // gesture
    CGPoint         gestureStart;
    NSInteger       gestureTaps;
}

- (Point)mouseLocForEvent:(GSEvent *)event;

- (void)toggleScreenSize;
- (void)toggleScreenScalingFilter;
- (void)scrollScreenView:(Direction)scroll;

- (void)swipeGesture:(Direction)direction;
- (void)twoFingerTapGesture:(GSEvent *)event;
- (void)twoFingerTapGestureDone;

@end