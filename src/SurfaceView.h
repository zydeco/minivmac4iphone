/** 
 *  @file SurfaceView.h
 *  A View that implements a CoreSurface buffer
 *  The buffer is created automatically in initWithFrame: with the size
 *  passed to it and 5-6-5 RGB format.
 *  You can also initialize with initWithFrame:pixelFormat: to specify
 *  another pixel format from a kPixelFormat* constant.
 *  The memory can be accessed with the pixels property, and after modifying
 *  it you must call setNeedsDisplay on it for it to update.
 */

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <CoreSurface/CoreSurface.h>
#import <QuartzCore/CALayer.h>
#import <GraphicsServices/GraphicsServices.h>

typedef const char* PixelFormat;
extern PixelFormat kPixelFormat565L;
extern PixelFormat kPixelFormatARGB;
/* untested:
extern PixelFormat kPixelFormatABGR;
extern PixelFormat kPixelFormatRGBA;
extern PixelFormat kPixelFormatA23B;
extern PixelFormat kPixelFormatA46B;
extern PixelFormat kPixelFormat1555;
extern PixelFormat kPixelFormatYUV2;
extern PixelFormat kPixelFormat4444;
extern PixelFormat kPixelFormat555L;
extern PixelFormat kPixelFormat565S;
extern PixelFormat kPixelFormatB23S;
extern PixelFormat kPixelFormat555S;
extern PixelFormat kPixelFormat155S;
extern PixelFormat kPixelFormatSVUY;
extern PixelFormat kPixelFormatV024;
extern PixelFormat kPixelFormat024Y;
extern PixelFormat kPixelFormat804V; /**/

@interface SurfaceView : UIView 
{
    CALayer*                surfaceLayer;
    void*                   pixels;
    PixelFormat             pixelFormat;
    CoreSurfaceBufferRef    surfaceBuffer;
    CGSize                  surfaceSize;
    CGRect                  fakeFrame;
}

@property (nonatomic, readonly) void *pixels;
@property (nonatomic, readonly) PixelFormat pixelFormat;
@property (nonatomic, readonly) int pixelSize;
@property (nonatomic, retain) NSString* magnificationFilter;
@property (nonatomic, retain) NSString* minificationFilter;

- (id)initWithFrame:(CGRect)frame pixelFormat:(PixelFormat)pxf;
- (id)initWithFrame:(CGRect)frame pixelFormat:(PixelFormat)pxf surfaceSize:(CGSize)size;
- (id)initWithFrame:(CGRect)frame pixelFormat:(PixelFormat)pxf scalingFilter:(NSString*)scalingFilter;
- (id)initWithFrame:(CGRect)frame pixelFormat:(PixelFormat)pxf surfaceSize:(CGSize)size scalingFilter:(NSString*)scalingFilter;
- (id)initWithFrame:(CGRect)frame pixelFormat:(PixelFormat)pxf magnificationFilter:(NSString*)magnificationFilter minificationFilter:(NSString*)minificationFilter;
- (id)initWithFrame:(CGRect)frame pixelFormat:(PixelFormat)pxf surfaceSize:(CGSize)size magnificationFilter:(NSString*)magnificationFilter minificationFilter:(NSString*)minificationFilter;
- (CGRect)fixFrame: (CGRect)frame;
@end
