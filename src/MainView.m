#import "MainView.h"

@implementation MainView

- (id)initWithFrame:(CGRect)rect {
    if (self = [super initWithFrame:rect]) {
        // initialization
        NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
        [self setBackgroundColor:[UIColor blackColor]];
        [self didChangePreferences:nil];
        
        // add screen view
        CGRect screenRect;
        if ([defaults boolForKey:@"ScreenSizeToFit"]) screenRect = kScreenRectFullScreen;
        else screenRect = kScreenRectRealSize;
        screenView = [[SurfaceView alloc] initWithFrame:screenRect pixelFormat:kPixelFormat565L surfaceSize:CGSizeMake(vMacScreenWidth, vMacScreenHeight) scalingFilter:kCAFilterLinear];
        [self addSubview:screenView];
        [screenView setUserInteractionEnabled:NO];
        _gScreenView = screenView;
        SurfaceScrnBuf = [screenView pixels];
        screenSizeToFit = [defaults boolForKey:@"ScreenSizeToFit"];
        screenPosition = [defaults integerForKey:@"ScreenPosition"];
        [self scrollScreenViewTo:screenPosition];
        mouseOffset.h = mouseOffset.v = 0;
        
        // add keyboard view
        keyboardView = [[KeyboardView alloc] initWithFrame:KeyboardViewFrameHidden];
        keyboardView.searchPaths = [[vMacApp sharedInstance] searchPaths];
        keyboardView.layout = [defaults objectForKey:@"KeyboardLayout"];
        [self addSubview:keyboardView];
        keyboardView.delegate = [vMacApp sharedInstance];
        
        // add insert disk view
        insertDiskView = [[InsertDiskView alloc] initWithFrame:InsertDiskViewFrameHidden];
        [self addSubview:insertDiskView];
        insertDiskView.diskDrive = [vMacApp sharedInstance];
        
        // add settings view
        settingsView = [[SettingsView alloc] initWithFrame:SettingsViewFrameHidden];
        [self addSubview:settingsView];
        
        // register for notifications
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didChangePreferences:) name:@"preferencesUpdated" object:nil];
    }
    return self;
}

- (void) dealloc {
    [super dealloc];
}

- (void)didChangePreferences:(NSNotification *)aNotification {
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
    trackpadMode = [defaults boolForKey:@"TrackpadMode"];
}

#if 0
#pragma mark -
#pragma mark Mouse
#endif

- (void)mouseDown:(GSEventRef)event {
    if (!screenSizeToFit) {
        // check to scroll screen
        CGPoint tapLoc = GSEventGetLocationInWindow(event).origin;
        CGPoint screenLoc = [screenView frame].origin;
        Direction scrollTo = 0;
        if (tapLoc.x < kScreenEdgeSize && screenLoc.x != 0.0) scrollTo |= dirLeft;
        if (tapLoc.y < kScreenEdgeSize && screenLoc.y != 0.0) scrollTo |= dirUp;
        if (tapLoc.x > (480-kScreenEdgeSize) && screenLoc.x == 0.0) scrollTo |= dirRight;
        if (tapLoc.y > (320-kScreenEdgeSize) && screenLoc.y == 0.0) scrollTo |= dirDown;
        if (scrollTo) {
            [self scrollScreenViewTo:scrollTo];
            return;
        }
    }
    
    Point loc = [self mouseLocForEvent:event];
    NSTimeInterval mouseTime = GSEventGetTimestamp(event);
    
    // help double clicking: click in the same place if it was fast and near
    if (((mouseTime - lastMouseClick) < MOUSE_DBLCLICK_TIME) &&
        (PointDistanceSq(loc, lastMouseLoc) < MOUSE_LOC_THRESHOLD)) {
        loc = lastMouseLoc;
        // start trackpad dragging
        if (trackpadMode) {
            trackpadDrag = YES;
            [_vmacAppSharedInstance setMouseButtonDown];
        }
    }
    
    if (trackpadMode) {
        trackpadClick = YES;
    } else {
        [_vmacAppSharedInstance setMouseLoc:loc];
        [_vmacAppSharedInstance performSelector:@selector(setMouseButtonDown) withObject:nil afterDelay:MOUSE_CLICK_DELAY];
    }
    
    lastMouseLoc = loc;
    lastMouseTime = lastMouseClick = mouseTime;
    [super mouseDown:event];
}

- (void)mouseUp:(GSEventRef)event {
    Point loc = [self mouseLocForEvent:event];
    NSTimeInterval mouseTime = GSEventGetTimestamp(event);
    
    // mouseUp in the same place if it's near enough
    if (PointDistanceSq(loc, lastMouseLoc) < MOUSE_LOC_THRESHOLD) {
        loc = lastMouseLoc;
    }
    
    if (trackpadMode) {
        if (trackpadClick && ((mouseTime - lastMouseTime) < TRACKPAD_CLICK_DELAY)) {
            [_vmacAppSharedInstance setMouseButtonDown];
            DoEmulateOneTick();
            DoEmulateOneTick();
        }
        trackpadClick = NO;
        trackpadDrag = NO;
        [_vmacAppSharedInstance setMouseButtonUp];
    } else {
        [_vmacAppSharedInstance setMouseLoc:loc];
        [_vmacAppSharedInstance performSelector:@selector(setMouseButtonUp) withObject:nil afterDelay:MOUSE_CLICK_DELAY];
    }
    
    lastMouseLoc = loc;
    [super mouseUp:event];
}

- (void)mouseDragged:(GSEventRef)event {
    if (trackpadMode) {
        trackpadClick = NO;
        [_vmacAppSharedInstance setMouseButton: trackpadDrag];
    } else {
        // mouseDown at current position NOW
        [vMacApp cancelPreviousPerformRequestsWithTarget:_vmacAppSharedInstance selector:@selector(setMouseButtonDown) object:nil];
        [_vmacAppSharedInstance setMouseButton:YES];
        DoEmulateOneTick();
    }
    
    NSTimeInterval mouseTime = GSEventGetTimestamp(event);
    Point loc = [self mouseLocForEvent:event];
    
    if (trackpadMode) {
        Point locDiff = loc;
        locDiff.h -= lastMouseLoc.h;
        locDiff.v -= lastMouseLoc.v;
        // acceleration
        NSTimeInterval timeDiff = 100 * (mouseTime - lastMouseTime);
        NSTimeInterval accel = 1 / (0.15 + ((timeDiff * timeDiff)/8));
        locDiff.h *= accel;
        locDiff.v *= accel;
        [_vmacAppSharedInstance moveMouse:locDiff];
    } else {
        [_vmacAppSharedInstance setMouseLoc:loc button:YES];
    }
    
    lastMouseTime = mouseTime;
    lastMouseLoc = loc;
    [super mouseDragged:event];
}

- (Point)mouseLocForEvent:(GSEventRef)event {
    CGRect r = GSEventGetLocationInWindow(event);
    Point pt;
    if (trackpadMode) {
        // same location
        pt.h = r.origin.x;
        pt.v = r.origin.y;
    } else if (screenSizeToFit) {
        // scale
        pt.h = r.origin.x * (vMacScreenWidth / 480.0);
        pt.v = r.origin.y * (vMacScreenHeight / 320.0);
    } else {
        // translate
        pt.h = r.origin.x - mouseOffset.h;
        pt.v = r.origin.y - mouseOffset.v;
    }
    return pt;
}

#if 0
#pragma mark -
#pragma mark Screen
#endif

- (void)toggleScreenSize {
    [UIView beginAnimations:nil context:nil];
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
    screenSizeToFit =! screenSizeToFit;
    if (screenSizeToFit) [screenView setFrame: kScreenRectFullScreen];
    else {
        [screenView setFrame: kScreenRectRealSize];
        [self scrollScreenViewTo: screenPosition];
    }
    [UIView endAnimations];
    [defaults setBool:screenSizeToFit forKey:@"ScreenSizeToFit"];
    [defaults synchronize];
}

- (void)scrollScreenViewTo:(Direction)scroll {
    if (screenSizeToFit) return;
    // calculate new position
    CGRect screenFrame = screenView.frame;
    if (scroll & dirDown) screenFrame.origin.y = 320-vMacScreenHeight;
    else if (scroll & dirUp) screenFrame.origin.y = 0.0;
    if (scroll & dirLeft) screenFrame.origin.x = 0.0;
    else if (scroll & dirRight) screenFrame.origin.x = 480-vMacScreenWidth;
    if (scroll != screenPosition) {
        screenPosition = scroll;
        NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
        [defaults setInteger:screenPosition forKey:@"ScreenPosition"];
        [defaults synchronize];
    }
    
    // set mouse offset
    mouseOffset.h = screenFrame.origin.x;
    mouseOffset.v = screenFrame.origin.y;
    
    // apply
    screenView.frame = screenFrame;
}

#if 0
#pragma mark -
#pragma mark Gestures
#endif

- (BOOL)canHandleGestures
{
    return YES;
}

- (void)gestureStarted:(GSEventRef)event {
    // cancel mouse button
    [vMacApp cancelPreviousPerformRequestsWithTarget:_vmacAppSharedInstance selector:@selector(setMouseButtonDown) object:nil];
    [_vmacAppSharedInstance setMouseButton:NO];
    
    // start gesture
    gestureStart = CGPointCenter(GSEventGetInnerMostPathPosition(event),
        GSEventGetOuterMostPathPosition(event));
}

- (void)gestureEnded:(GSEventRef)event {
    CGPoint gestureEnd = CGPointCenter(GSEventGetInnerMostPathPosition(event),
        GSEventGetOuterMostPathPosition(event));
    
    // process gesture (relative to landscape orientation)
    Direction swipeDirection = 0;
    CGPoint delta = CGPointMake(gestureStart.x-gestureEnd.x, gestureStart.y-gestureEnd.y);
    if (delta.x > kSwipeThresholdHorizontal)  swipeDirection |= dirLeft;
    if (delta.x < -kSwipeThresholdHorizontal) swipeDirection |= dirRight;
    if (delta.y > kSwipeThresholdVertical)    swipeDirection |= dirUp;
    if (delta.y < -kSwipeThresholdVertical)   swipeDirection |= dirDown;
    
    if (swipeDirection) [self twoFingerSwipeGesture:swipeDirection];
    else [self twoFingerTapGesture:event];
}

- (void)twoFingerSwipeGesture:(Direction)direction {
    if (direction == dirDown)
        [keyboardView hide];
    else if (direction == dirUp)
        [keyboardView show];
    else if (direction == dirLeft)
        [insertDiskView show];
    else if (direction == dirRight)
        [settingsView show];
}

- (void)twoFingerTapGesture:(GSEventRef)event {
    [self toggleScreenSize];
}

@end