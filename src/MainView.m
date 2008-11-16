#import "MainView.h"

@implementation MainView

- (id)initWithFrame:(CGRect)rect {
    if (self = [super initWithFrame:rect]) {
        // initialization
        NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
        [self setBackgroundColor:[UIColor blackColor]];
        
        // add screen view
        NSString* scalingFilter = kCAFilterNearest;
        if ([defaults boolForKey:@"ScreenAntialiasing"]) scalingFilter = kCAFilterLinear;
        screenView = [[SurfaceView alloc] initWithFrame:CGRectMake(0, 0, 480, 320) pixelFormat:kPixelFormat565L surfaceSize:CGSizeMake(512, 342) scalingFilter:scalingFilter];
        [self addSubview:screenView];
        [screenView setUserInteractionEnabled:NO];
        _gScreenView = screenView;
        SurfaceScrnBuf = [screenView pixels];
        screenSizeToFit = ![defaults boolForKey:@"ScreenSizeToFit"];
        [self toggleScreenSize]; // ugly, I know
        mouseOffset.h = mouseOffset.v = 0;
        
        // add keyboard view
        keyboardView = [[KeyboardView alloc] initWithFrame:KeyboardViewFrameHidden];
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
    }
    return self;
}

- (void) dealloc {
    [super dealloc];
}

#if 0
#pragma mark -
#pragma mark Mouse
#endif

- (void)mouseDown:(GSEvent *)event {
    if (!screenSizeToFit) {
        // check to scroll screen
        CGPoint tapLoc = GSEventGetLocationInWindow(event).origin;
        CGPoint screenLoc = [screenView frame].origin;
        Direction scrollTo = 0;
        if (tapLoc.x < SCREEN_EDGE_SIZE && screenLoc.x != 0.0) scrollTo |= dirLeft;
        if (tapLoc.y < SCREEN_EDGE_SIZE && screenLoc.y != 0.0) scrollTo |= dirUp;
        if (tapLoc.x > (480-SCREEN_EDGE_SIZE) && screenLoc.x == 0.0) scrollTo |= dirRight;
        if (tapLoc.y > (320-SCREEN_EDGE_SIZE) && screenLoc.y == 0.0) scrollTo |= dirDown;
        if (scrollTo) {
            [self scrollScreenView:scrollTo];
            return;
        }
    }
    
    Point loc = [self mouseLocForEvent:event];
    
    // mouse button must be up
    [[_vmacAppSharedInstance class] cancelPreviousPerformRequestsWithTarget:_vmacAppSharedInstance selector:@selector(setMouseButtonUp) object:nil];
    [_vmacAppSharedInstance setMouseButton:NO];
    
    #if defined(MOUSE_DBLCLICK_HELPER) && MOUSE_DBLCLICK_HELPER
    NSTimeInterval mouseTime = GSEventGetTimestamp(event);
    // help double clicking: click in the same place if it was fast and near
    if (((mouseTime - lastMouseTime) < MOUSE_DBLCLICK_TIME) &&
        (PointDistanceSq(loc, lastMouseLoc) < MOUSE_LOC_TRESHOLD)) {
        loc = lastMouseLoc;
    }
    lastMouseLoc = loc;
    lastMouseTime = mouseTime;
    #endif
    
    [_vmacAppSharedInstance setMouseLoc:loc];
    [_vmacAppSharedInstance performSelector:@selector(setMouseButtonDown) withObject:nil afterDelay:MOUSE_CLICK_DELAY];
    [super mouseDown:event];
}

- (void)mouseUp:(GSEvent *)event {
    Point loc = [self mouseLocForEvent:event];
    
    #if defined(MOUSE_DBLCLICK_HELPER) && MOUSE_DBLCLICK_HELPER
    // mouseUp in the same place if it's near enough
    if (PointDistanceSq(loc, lastMouseLoc) < MOUSE_LOC_TRESHOLD) {
        loc = lastMouseLoc;
    }
    lastMouseLoc = loc;
    #endif
    
    [_vmacAppSharedInstance setMouseLoc:loc];
    [_vmacAppSharedInstance performSelector:@selector(setMouseButtonUp) withObject:nil afterDelay:MOUSE_CLICK_DELAY];
    [super mouseUp:event];
}

- (void)mouseDragged:(GSEvent *)event {
    // mouse button must be pressed
    [[_vmacAppSharedInstance class] cancelPreviousPerformRequestsWithTarget:_vmacAppSharedInstance selector:@selector(setMouseButtonDown) object:nil];
    [_vmacAppSharedInstance setMouseButton:YES];
    
    #if defined(MOUSE_DBLCLICK_HELPER) && MOUSE_DBLCLICK_HELPER
    lastMouseTime = 0;
    #endif
    
    [_vmacAppSharedInstance setMouseLoc:[self mouseLocForEvent:event] button:YES];
    [super mouseDragged:event];
}

- (Point)mouseLocForEvent:(GSEvent *)event {
    CGRect r = GSEventGetLocationInWindow(event);
    Point pt;
    if (screenSizeToFit) {
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
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
    screenSizeToFit =! screenSizeToFit;
    if (screenSizeToFit) [screenView setFrame: CGRectMake(0, 0, 480, 320)];
    else [screenView setFrame: CGRectMake(0, 0, 512, 342)];
    [defaults setBool:screenSizeToFit forKey:@"ScreenSizeToFit"];
    [defaults synchronize];
}

- (void)toggleScreenScalingFilter {
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
    if ([screenView.minificationFilter isEqual:kCAFilterLinear]) {
        screenView.minificationFilter =  kCAFilterNearest;
        screenView.magnificationFilter = kCAFilterNearest;
        [defaults setBool:NO forKey:@"ScreenAntialiasing"];
    } else {
        screenView.minificationFilter =  kCAFilterLinear;
        screenView.magnificationFilter = kCAFilterLinear;
        [defaults setBool:YES forKey:@"ScreenAntialiasing"];
    }
    [defaults synchronize];
}

- (void)scrollScreenView:(Direction)scroll {
    // calculate new position
    CGRect screenFrame = screenView.frame;
    if (scroll & dirDown) screenFrame.origin.y = 320-342;
    else if (scroll & dirUp) screenFrame.origin.y = 0.0;
    if (scroll & dirLeft) screenFrame.origin.x = 0.0;
    else if (scroll & dirRight) screenFrame.origin.x = 480-512;
    
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

- (void)gestureStarted:(GSEvent *)event {
    // cancel mouse button
    [[_vmacAppSharedInstance class] cancelPreviousPerformRequestsWithTarget:_vmacAppSharedInstance selector:@selector(setMouseButtonDown) object:nil];
    [_vmacAppSharedInstance setMouseButton:NO];
    
    // start gesture
    gestureStart = CGPointCenter(GSEventGetInnerMostPathPosition(event),
        GSEventGetOuterMostPathPosition(event));
}

- (void)gestureEnded:(GSEvent *)event {
    CGPoint gestureEnd = CGPointCenter(GSEventGetInnerMostPathPosition(event),
        GSEventGetOuterMostPathPosition(event));
    
    // process gesture (relative to landscape orientation)
    Direction swipeDirection = 0;
    CGPoint delta = CGPointMake(gestureStart.x-gestureEnd.x, gestureStart.y-gestureEnd.y);
    if (delta.x > 100.0)    swipeDirection |= dirLeft;
    if (delta.x < -100.0)   swipeDirection |= dirRight;
    if (delta.y > 70.0)    swipeDirection |= dirUp;
    if (delta.y < -70.0)   swipeDirection |= dirDown;
    
    if (swipeDirection) [self swipeGesture:swipeDirection];
    else [self twoFingerTapGesture:event];
}

- (void)swipeGesture:(Direction)direction {
    if (direction == dirDown)
        [keyboardView hide];
    else if (direction == dirUp)
        [keyboardView show];
    else if (direction == dirLeft)
        [insertDiskView show];
    else if (direction == dirRight)
        [settingsView show];
}

- (void)twoFingerTapGesture:(GSEvent *)event {
    if (gestureTaps == 0)
        [self performSelector:@selector(twoFingerTapGestureDone) withObject:nil afterDelay:MOUSE_DBLCLICK_TIME];
    gestureTaps++;
}

- (void)twoFingerTapGestureDone {
    if (gestureTaps == 1) [self toggleScreenSize];
    if (gestureTaps == 2) [self toggleScreenScalingFilter];
    gestureTaps = 0;
}

@end