#ifdef __OBJC__

#import <UIKit/UIKit.h>

// stuff missing from devteam headers

#define UIInterfaceOrientationLandscapeRight 3

extern NSString * const kCAFilterLinear; // XXX: from QuartzCore

@interface UIApplication ()
- (void) setStatusBarOrientation:(int)fp8 animated:(BOOL)fp12;
@end

@interface UIImage ()
+ (id)imageWithData:(id)fp8;
+ (id)imageWithCGImage:(struct CGImage *)fp8;
- (CGImageRef)CGImage;
@end

NSData *UIImagePNGRepresentation(UIImage *image);

#endif