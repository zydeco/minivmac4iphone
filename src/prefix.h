#ifdef __OBJC__

#import <UIKit/UIKit.h>

// stuff missing from devteam headers

#define UIInterfaceOrientationLandscapeRight 3

@interface UIApplication (extensions)
- (void) setStatusBarOrientation:(int)fp8 animated:(BOOL)fp12;
@end

#endif