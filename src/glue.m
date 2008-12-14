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
#import <AudioToolbox/AudioToolbox.h>
#import <sys/time.h>
#import "vMacApp.h"
#import "DATE2SEC.h"

blnr SpeedStopped = YES;
NSInteger numInsertedDisks;
short* SurfaceScrnBuf;
short* pixelConversionTable;
id _gScreenView;
ui5b MacDateDiff;
#define UsecPerSec 1000000
#define MyInvTimeStep 16626 /* UsecPerSec / 60.14742 */
LOCALVAR ui5b TrueEmulatedTime = 0;
ui5b CurEmulatedTime = 0;
LOCALVAR ui5b OnTrueTime = 0;
LOCALVAR ui5b LastTimeSec, NextTimeSec;
LOCALVAR ui5b LastTimeUsec, NextTimeUsec;

IMPORTFUNC blnr ScreenFindChanges(si3b TimeAdjust, si4b *top, si4b *left, si4b *bottom, si4b *right);

#if 0
#pragma mark -
#pragma mark Warnings
#endif

GLOBALPROC WarnMsgUnsupportedROM(void) {
    [_vmacAppSharedInstance warnMessage:NSLocalizedString(@"WarnUnsupportedROM", nil)];
}

#if DetailedAbormalReport
GLOBALPROC WarnMsgAbnormal(char *s)
{
    [_vmacAppSharedInstance warnMessage:[NSString stringWithFormat:NSLocalizedString(@"WarnAbnormalSituationDetailed", nil), s]];
}
#else
GLOBALPROC WarnMsgAbnormal(void)
{
    [_vmacAppSharedInstance warnMessage:NSLocalizedString(@"WarnAbnormalSituation", nil)];
}
#endif

GLOBALPROC WarnMsgCorruptedROM(void)
{
    [_vmacAppSharedInstance warnMessage:NSLocalizedString(@"WarnCorruptedROM", nil)];
}

#if 0
#pragma mark -
#pragma mark Screen
#endif

void updateScreen (si3b TimeAdjust)
{
    si4b top, left, bottom, right;
    
    // has the screen changed?
    if (!ScreenFindChanges(TimeAdjust, &top, &left, &bottom, &right)) return;
    
    // convert the pixels
    unsigned char *vmacScrnBuf = screencomparebuff;
    int startByte = ((vMacScreenWidth * top) + left) / 8;
    int endByte = ((vMacScreenWidth * bottom) + right) / 8;
    register short *scrnBuf = SurfaceScrnBuf + (8 * startByte);
    for(register int i = startByte; i < endByte; i++) {
        memcpy(scrnBuf, &pixelConversionTable[8 * vmacScrnBuf[i]], 16);
        scrnBuf += 8;
    }
    
    objc_msgSend(_gScreenView, @selector(setNeedsDisplay));
}


#if 0
#pragma mark -
#pragma mark Sound
#endif

#if MySoundEnabled
#define SOUND_SAMPLERATE 22255
#define kLn2SoundBuffers 4 /* kSoundBuffers must be a power of two, must have at least 2^2 buffers */
#define kSoundBuffers (1 << kLn2SoundBuffers)
#define kSoundBuffMask (kSoundBuffers - 1)
#define kLn2BuffLen 9
#define kLnBuffSz (kLn2SoundBuffers + kLn2BuffLen)
#define My_Sound_Len (1UL << kLn2BuffLen)
#define kBufferSize (1UL << kLnBuffSz)
#define kBufferMask (kBufferSize - 1)
#define dbhBufferSize (kBufferSize + SOUND_LEN)
#define DesiredMinFilledSoundBuffs 4

static int curFillBuffer = 0;
static int curReadBuffer = 0;
static int numFullBuffers = 0;
static char sndBuffer[kSoundBuffers][SOUND_LEN];

#define FillWithSilence(p,n,v) for (int fws_i = n; --fws_i >= 0;) *p++ = v

struct {
    bool                          mIsInitialized;
    bool                          mIsRunning;
    AudioQueueRef                 mQueue;
    AudioStreamBasicDescription   mDataFormat;
    AudioQueueBufferRef           mBuffers[kSoundBuffers];
} aq;

LOCALPROC MySound_SecondNotify(void)
{
    if (!aq.mIsRunning) return;
    if (numFullBuffers > DesiredMinFilledSoundBuffs) {
        ++CurEmulatedTime;
    } else if (numFullBuffers < DesiredMinFilledSoundBuffs) {
        --CurEmulatedTime;
    }
}

void MySound_Callback (void *data, AudioQueueRef mQueue, AudioQueueBufferRef mBuffer) {
    mBuffer->mAudioDataByteSize = SOUND_LEN;
    char *mAudioData = mBuffer->mAudioData;
    if (numFullBuffers == 0) {
        FillWithSilence(mAudioData, SOUND_LEN, 0x80);
    } else {
        memcpy(mAudioData, sndBuffer[curReadBuffer], SOUND_LEN);
        numFullBuffers--;
        curReadBuffer = (curReadBuffer+1) & kSoundBuffMask;
    }
    AudioQueueEnqueueBuffer(mQueue, mBuffer, 0, NULL);
}

bool MySound_Init(void) {
    OSStatus err;
    bzero(&aq, sizeof aq);
    
    // create queue
    aq.mDataFormat.mSampleRate = SOUND_SAMPLERATE;
    aq.mDataFormat.mFormatID = kAudioFormatLinearPCM;
    aq.mDataFormat.mFormatFlags = kAudioFormatFlagIsPacked;
    aq.mDataFormat.mBytesPerPacket = 1;
    aq.mDataFormat.mFramesPerPacket = 1;
    aq.mDataFormat.mBytesPerFrame = 1;
    aq.mDataFormat.mChannelsPerFrame = 1;
    aq.mDataFormat.mBitsPerChannel = 8;
    aq.mDataFormat.mReserved = 0;
    err = AudioQueueNewOutput(&aq.mDataFormat, MySound_Callback, NULL, CFRunLoopGetMain(), kCFRunLoopCommonModes, 0, &aq.mQueue);
    if (err != noErr) NSLog(@"Error %d creating audio queue", err);
    
    // create buffers
    for (int i=0; i<kSoundBuffers; i++) {
        AudioQueueAllocateBuffer(aq.mQueue, SOUND_LEN, &aq.mBuffers[i]);
        MySound_Callback(NULL, aq.mQueue, aq.mBuffers[i]);
    }
    
    aq.mIsInitialized = true;
    return trueblnr;
}

GLOBALPROC MySound_Start (void) {
    if (!aq.mIsInitialized || aq.mIsRunning) return;
    AudioQueueStart(aq.mQueue, NULL);
    aq.mIsRunning = true;
}

GLOBALPROC MySound_Stop (void) {
    if (!aq.mIsRunning || !aq.mIsInitialized) return;
    AudioQueueStop(aq.mQueue, false);
    aq.mIsRunning = false;
}

GLOBALFUNC ui3p GetCurSoundOutBuff(void) {
    if (!aq.mIsRunning) return nullpr;
    if (numFullBuffers == kSoundBuffers) return nullpr;
    curFillBuffer = (curFillBuffer+1) & kSoundBuffMask;
    numFullBuffers ++;
    return sndBuffer[curFillBuffer];
}
#else

GLOBALFUNC ui3p GetCurSoundOutBuff(void) {
    return nullpr;
}

#endif

#if 0
#pragma mark -
#pragma mark Emulation
#endif

LOCALPROC IncrNextTime(void)
{
    /* increment NextTime by one tick */
    NextTimeUsec += MyInvTimeStep;
    if (NextTimeUsec >= UsecPerSec) {
        NextTimeUsec -= UsecPerSec;
        NextTimeSec += 1;
    }
}

LOCALPROC InitNextTime(void)
{
    NextTimeSec = LastTimeSec;
    NextTimeUsec = LastTimeUsec;
    IncrNextTime();
}

LOCALPROC GetCurrentTicks(void)
{
    struct timeval t;
    gettimeofday(&t, NULL);
    LastTimeSec = (ui5b)t.tv_sec;
    LastTimeUsec = (ui5b)t.tv_usec;
}

void StartUpTimeAdjust (void)
{
    GetCurrentTicks();
    InitNextTime();
}

LOCALFUNC si5b GetTimeDiff(void)
{
    return ((si5b)(LastTimeSec - NextTimeSec)) * UsecPerSec
        + ((si5b)(LastTimeUsec - NextTimeUsec));
}

LOCALFUNC blnr CheckDateTime (void)
{
    ui5b NewMacDate = time(NULL) + MacDateDiff;
    if (NewMacDate != CurMacDateInSeconds) {
        CurMacDateInSeconds = NewMacDate;
        return trueblnr;
    }
    return falseblnr;
}

LOCALPROC UpdateTrueEmulatedTime(void)
{
    si5b TimeDiff;
    
    GetCurrentTicks();
    
    TimeDiff = GetTimeDiff();
    if (TimeDiff >= 0) {
        if (TimeDiff > 4 * MyInvTimeStep) {
            /* emulation interrupted, forget it */
            ++TrueEmulatedTime;
            InitNextTime();
        } else {
            do {
                ++TrueEmulatedTime;
                IncrNextTime();
                TimeDiff -= UsecPerSec;
            } while (TimeDiff >= 0);
        }
    } else if (TimeDiff < - 2 * MyInvTimeStep) {
        /* clock goofed if ever get here, reset */
        InitNextTime();
    }
}

GLOBALFUNC blnr ExtraTimeNotOver(void)
{
    UpdateTrueEmulatedTime();
    return TrueEmulatedTime == OnTrueTime;
}

LOCALPROC RunEmulatedTicksToTrueTime(void)
{
    si3b n;
    SpeedLimit = falseblnr;
    if (CheckDateTime()) {
    #if MySoundEnabled
            MySound_SecondNotify();
    #endif
    }
    
    n = OnTrueTime - CurEmulatedTime;
    if (n > 0) {
        if (n > 8) {
            /* emulation not fast enough */
            n = 8;
            CurEmulatedTime = OnTrueTime - n;
        }
        
        do {
            DoEmulateOneTick();
            ++CurEmulatedTime;
        } while (ExtraTimeNotOver() && (--n > 0));
        
        updateScreen(n);
    }
}

void runTick (CFRunLoopTimerRef timer, void* info)
{
    if (SpeedStopped) return;
    
    UpdateTrueEmulatedTime();
    OnTrueTime = TrueEmulatedTime;
    RunEmulatedTicksToTrueTime();
}

#if 0
#pragma mark -
#pragma mark Misc
#endif

GLOBALPROC MyMoveBytes(anyp srcPtr, anyp destPtr, si5b byteCount)
{
    memcpy((char *)destPtr, (char *)srcPtr, byteCount);
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

