/** 
 *  @file SurfaceView.m
 *  Implementation of SurfaceView
 */

#import "SurfaceView.h"

PixelFormat kPixelFormat565L = "565L";
PixelFormat kPixelFormatARGB = "ARGB";

// untested, found in QuartzCore
PixelFormat kPixelFormatABGR = "ABGR";
PixelFormat kPixelFormatRGBA = "RGBA";
PixelFormat kPixelFormatA23B = "a23b";
PixelFormat kPixelFormatA46B = "a46b";
PixelFormat kPixelFormat1555 = "1555";
PixelFormat kPixelFormatYUV2 = "yuv2";
PixelFormat kPixelFormat4444 = "4444";
PixelFormat kPixelFormat555L = "555L";
PixelFormat kPixelFormat565S = "565S";
PixelFormat kPixelFormatB23S = "b23S";
PixelFormat kPixelFormat555S = "555S";
PixelFormat kPixelFormat155S = "155s";
PixelFormat kPixelFormatSVUY = "svuy";
PixelFormat kPixelFormatV024 = "v024";
PixelFormat kPixelFormat024Y = "024y";
PixelFormat kPixelFormat804V = "804V";

// kCAFilterNearest, kCAFilterLinear
#define kDefaultScalingFilter kCAFilterLinear 

@implementation SurfaceView

- (id)initWithFrame:(CGRect)frame {
    return [self initWithFrame:frame pixelFormat:kPixelFormat565L surfaceSize:frame.size magnificationFilter:kDefaultScalingFilter minificationFilter:kDefaultScalingFilter];
}

- (id)initWithFrame:(CGRect)frame surfaceSize:(CGSize)size {
    return [self initWithFrame:frame pixelFormat:kPixelFormat565L surfaceSize:size magnificationFilter:kDefaultScalingFilter minificationFilter:kDefaultScalingFilter];
}

- (id)initWithFrame:(CGRect)frame pixelFormat:(PixelFormat)pxf {
    return [self initWithFrame:frame pixelFormat:pxf surfaceSize:frame.size magnificationFilter:kDefaultScalingFilter minificationFilter:kDefaultScalingFilter];
}

- (id)initWithFrame:(CGRect)frame pixelFormat:(PixelFormat)pxf surfaceSize:(CGSize)size {
    return [self initWithFrame:frame pixelFormat:pxf surfaceSize:size magnificationFilter:kDefaultScalingFilter minificationFilter:kDefaultScalingFilter];
}

- (id)initWithFrame:(CGRect)frame pixelFormat:(PixelFormat)pxf scalingFilter:(NSString*)scalingFilter {
    return [self initWithFrame:frame pixelFormat:pxf surfaceSize:frame.size magnificationFilter:scalingFilter minificationFilter:scalingFilter];
}

- (id)initWithFrame:(CGRect)frame pixelFormat:(PixelFormat)pxf surfaceSize:(CGSize)size scalingFilter:(NSString*)scalingFilter {
    return [self initWithFrame:frame pixelFormat:pxf surfaceSize:size magnificationFilter:scalingFilter minificationFilter:scalingFilter];
}

- (id)initWithFrame:(CGRect)frame pixelFormat:(PixelFormat)pxf magnificationFilter:(NSString*)magnificationFilter minificationFilter:(NSString*)minificationFilter {
    return [self initWithFrame:frame pixelFormat:pxf surfaceSize:frame.size magnificationFilter:magnificationFilter minificationFilter:minificationFilter];
}

// real initializer
- (id)initWithFrame:(CGRect)frame pixelFormat:(PixelFormat)pxf surfaceSize:(CGSize)size magnificationFilter:(NSString*)magnificationFilter minificationFilter:(NSString*)minificationFilter {
    // set values
    pixelFormat = pxf;
    int p = self.pixelSize;
    surfaceSize = size;
    if (self = [super initWithFrame:frame]) {
        // Create Surface
        int w = surfaceSize.width, h = surfaceSize.height;
        surfaceBuffer = CoreSurfaceBufferCreate((CFDictionaryRef)[NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithInt:w],         kCoreSurfaceBufferWidth,
            [NSNumber numberWithInt:h],         kCoreSurfaceBufferHeight,
            [NSNumber numberWithInt:*(int*)pxf],kCoreSurfaceBufferPixelFormat,
            [NSNumber numberWithInt:p*w*h],     kCoreSurfaceBufferAllocSize,
            [NSNumber numberWithBool:YES],      kCoreSurfaceBufferGlobal,
            [NSNumber numberWithInt:p*w],       kCoreSurfaceBufferPitch,
            @"PurpleGFXMem",                    kCoreSurfaceBufferMemoryRegion,
            nil]);
        
        // Create layer for surface
        CoreSurfaceBufferLock(surfaceBuffer, 3);
        surfaceLayer = [[CALayer layer] retain];
        [surfaceLayer setMagnificationFilter:magnificationFilter];
        [surfaceLayer setMinificationFilter:minificationFilter];
        [surfaceLayer setEdgeAntialiasingMask:15];
        [surfaceLayer setFrame:[self fixFrame:frame]];
        [surfaceLayer setContents:surfaceBuffer];
        [surfaceLayer setOpaque:YES];
        [[self _layer] addSublayer:surfaceLayer];
        CoreSurfaceBufferUnlock(surfaceBuffer);
        
        // Get base address
        pixels = CoreSurfaceBufferGetBaseAddress(surfaceBuffer);
    }
    return self;
}

- (void)dealloc {
    [surfaceLayer release];
    [super dealloc];
}

- (void)drawRect:(CGRect)rect {
    // Do not remove this empty function, or you'll break it
}

#if 0
#pragma mark -
#pragma mark Accessors
#endif

- (void*)pixels {
    return pixels;
}

- (PixelFormat)pixelFormat {
    return pixelFormat;
}

- (int)pixelSize {
    if (pixelFormat == kPixelFormat565L) return 2;
    if (pixelFormat == kPixelFormat1555) return 2;
    if (pixelFormat == kPixelFormat4444) return 2;
    if (pixelFormat == kPixelFormat555L) return 2;
    if (pixelFormat == kPixelFormat565S) return 2;
    if (pixelFormat == kPixelFormat555S) return 2;
    if (pixelFormat == kPixelFormat155S) return 2;
    if (pixelFormat == kPixelFormatARGB) return 4;
    if (pixelFormat == kPixelFormatABGR) return 4;
    if (pixelFormat == kPixelFormatRGBA) return 4;
    // unknown, 4 should be safe
    return 4;
    /*
    if (pixelFormat == kPixelFormatA23B) return 4;
    if (pixelFormat == kPixelFormatA46B) return 4;
    if (pixelFormat == kPixelFormatYUV2) return 4;
    if (pixelFormat == kPixelFormatB23S) return 4;
    if (pixelFormat == kPixelFormatSVUY) return 4;
    if (pixelFormat == kPixelFormatV024) return 4;
    if (pixelFormat == kPixelFormat024Y) return 4;
    if (pixelFormat == kPixelFormat804V) return 4;
    */
}

- (CGRect)frame {
    return fakeFrame;
}

- (void)setFrame:(CGRect)frame {
    fakeFrame = frame;
    frame = [self fixFrame:frame];
    [super setFrame:frame];
    if (surfaceLayer) [surfaceLayer setFrame:frame];
}

- (NSString*)magnificationFilter {
    return [surfaceLayer magnificationFilter];
}

- (void)setMagnificationFilter:(NSString*)magnificationFilter {
    [surfaceLayer setMagnificationFilter:magnificationFilter];
}

- (NSString*)minificationFilter {
    return [surfaceLayer minificationFilter];
}

- (void)setMinificationFilter:(NSString*)minificationFilter {
    [surfaceLayer setMinificationFilter:minificationFilter];
}

- (CGRect)fixFrame: (CGRect)frame {
    // CoreSurface is sofa king buggy
    int p = self.pixelSize;
    frame.origin.x /= p;
    frame.origin.y /= p;
    if (frame.size.height == surfaceSize.height) frame.size.height += 1;
    if (frame.size.width == surfaceSize.width) frame.size.width += 1;
    return frame;
}



@end
