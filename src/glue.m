/*
 Mini vMac for iPhone
 Copyright (c) 2008, Jesús A. Álvarez

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2
 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#import <UIKit/UIKit.h>
#import "vMacApp.h"
#import "DATE2SEC.h"

blnr SpeedStopped = YES;
blnr ScreenNeedsUpdate = YES;
NSInteger numInsertedDisks;
short* SurfaceScrnBuf;
id _gScreenView;

#if 0
#pragma mark -
#pragma mark Warnings
#endif

GLOBALPROC WarnMsgUnsupportedROM(void) {
    [_vmacAppSharedInstance warnMessage:@"Unsupported ROM"];
}

#if DetailedAbormalReport
GLOBALPROC WarnMsgAbnormal(char *s)
{
    [_vmacAppSharedInstance warnMessage:[NSString stringWithFormat:@"Abnormal Situation: %s", s]];
}
#else
GLOBALPROC WarnMsgAbnormal(void)
{
    [_vmacAppSharedInstance warnMessage:@"Abnormal Situation"];
}
#endif

GLOBALPROC WarnMsgCorruptedROM(void)
{
    [_vmacAppSharedInstance warnMessage:@"Corrupted ROM"];
}

#if 0
#pragma mark -
#pragma mark Emulation
#endif

GLOBALFUNC blnr ExtraTimeNotOver(void)
{
    NSLog(@"This code is never reached");
    return falseblnr;
}

GLOBALPROC HaveChangedScreenBuff(si4b top, si4b left, si4b bottom, si4b right)
{
    ScreenNeedsUpdate = trueblnr;
}

void updateScreen (CFRunLoopTimerRef timer, void* info)
{
    Screen_Draw(MyFrameSkip);
    if (!ScreenNeedsUpdate) return;
    ScreenNeedsUpdate = falseblnr;
    
    // convert the pixels
    char *vmacScrnBuf = screencomparebuff;
    short *scrnBuf = SurfaceScrnBuf;
    register int currentPixels;
    for(register int i=0; i < (vMacScreenWidth*vMacScreenHeight)/8; i++) {
        currentPixels = vmacScrnBuf[i];
        *scrnBuf++ = (currentPixels & 0x80) ? 0x0000 : 0xFFFF;
        *scrnBuf++ = (currentPixels & 0x40) ? 0x0000 : 0xFFFF;
        *scrnBuf++ = (currentPixels & 0x20) ? 0x0000 : 0xFFFF;
        *scrnBuf++ = (currentPixels & 0x10) ? 0x0000 : 0xFFFF;
        *scrnBuf++ = (currentPixels & 0x08) ? 0x0000 : 0xFFFF;
        *scrnBuf++ = (currentPixels & 0x04) ? 0x0000 : 0xFFFF;
        *scrnBuf++ = (currentPixels & 0x02) ? 0x0000 : 0xFFFF;
        *scrnBuf++ = (currentPixels & 0x01) ? 0x0000 : 0xFFFF;
    }
    
    objc_msgSend(_gScreenView, @selector(setNeedsDisplay));
}

#if 0
#pragma mark -
#pragma mark Misc
#endif

void runTick (CFRunLoopTimerRef timer, void* info)
{
    if (SpeedStopped) return;
    static int i = 0;
    DoEmulateOneTick();
#if MyFrameSkip
    if (++i%MyFrameSkip == 0) updateScreen(nil, nil);
#else
    UnusedParam(i);
    updateScreen(nil, nil);
#endif
}


#if 0
#pragma mark -
#pragma mark Floppy Driver
#endif

GLOBALFUNC si4b vSonyRead(void *Buffer, ui4b Drive_No, ui5b Sony_Start, ui5b *Sony_Count)
{
    return [_vmacAppSharedInstance readFromDrive:Drive_No start:Sony_Start count:Sony_Count buffer:Buffer];
}

GLOBALFUNC si4b vSonyWrite(void *Buffer, ui4b Drive_No, ui5b Sony_Start, ui5b *Sony_Count)
{
    return [_vmacAppSharedInstance writeToDrive:Drive_No start:Sony_Start count:Sony_Count buffer:Buffer];
}

GLOBALFUNC si4b vSonyGetSize(ui4b Drive_No, ui5b *Sony_Count)
{
    return [_vmacAppSharedInstance sizeOfDrive:Drive_No count:Sony_Count];
}

GLOBALFUNC si4b vSonyEject(ui4b Drive_No) {
    return [_vmacAppSharedInstance ejectDrive:Drive_No];
}

#if 0
#pragma mark -
#pragma mark Sound
#endif

#if MySoundEnabled
#define SOUND_SAMPLERATE 22255
#define kLn2SoundBuffers 4 /* kSoundBuffers must be a power of two */
#define kSoundBuffers (1 << kLn2SoundBuffers)
#define kSoundBuffMask (kSoundBuffers - 1)
#define DesiredMinFilledSoundBuffs 3
/*
 if too big then sound lags behind emulation.
 if too small then sound will have pauses.
 */

#define kLn2BuffLen 9
#define kLnBuffSz (kLn2SoundBuffers + kLn2BuffLen)
#define My_Sound_Len (1UL << kLn2BuffLen)
#define kBufferSize (1UL << kLnBuffSz)
#define kBufferMask (kBufferSize - 1)
#define dbhBufferSize (kBufferSize + SOUND_LEN)
#define FillWithSilence(p,n,v) for (int fws_i = n; --i >= 0;) *p++ = v
static const int kNumberBuffers = 3;
struct AQPlayerState {
    AudioStreamBasicDescription   mDataFormat;
    AudioQueueRef                 mQueue;
    AudioQueueBufferRef           mBuffers[kSoundBuffers];
    AudioFileID                   mAudioFile;
    UInt32                        bufferByteSize;
    SInt64                        mCurrentPacket;
    UInt32                        mNumPacketsToRead;
    AudioStreamPacketDescription  *mPacketDescs;
    bool                          mIsRunning;
} aq;
void MySoundCallback(void *userData, AudioQueueRef queue, AudioQueueBufferRef buffer);

void MySound_Start(void)
{
    NSLog(@"SoundStart");
    AudioQueueStart(aq.mQueue, NULL);
    aq.mIsRunning = true;
}

GLOBALFUNC void SoundInit(void)
{
    OSStatus err;
    NSLog(@"SoundInit");
    aq.mDataFormat.mSampleRate = SOUND_SAMPLERATE;
    aq.mDataFormat.mFormatID = 'lpcm';
    aq.mDataFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsBigEndian | kAudioFormatFlagIsPacked;
    aq.mDataFormat.mBytesPerPacket = 1;
    aq.mDataFormat.mFramesPerPacket = 1;
    aq.mDataFormat.mBytesPerFrame = 1;
    aq.mDataFormat.mChannelsPerFrame = 1;
    aq.mDataFormat.mBitsPerChannel = 8;
    aq.mDataFormat.mReserved = 0;
    
    err = AudioQueueNewOutput(&(aq.mDataFormat), MySoundCallback, NULL, CFRunLoopGetMain(), kCFRunLoopCommonModes, 0, &(aq.mQueue));
    if (err != noErr) NSLog(@"Error %d creating audio queue", err);
}

void MySoundCallback(void *userData, AudioQueueRef queue, AudioQueueBufferRef buffer)
{
    NSLog(@"SoundCB");
}

GLOBALFUNC ui3p GetCurSoundOutBuff(void)
{
    return nullpr;
}
#else

GLOBALFUNC ui3p GetCurSoundOutBuff(void) {
    return nullpr;
}

#endif
