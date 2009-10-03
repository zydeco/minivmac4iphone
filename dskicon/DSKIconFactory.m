/*
 * dskicon - utility to create icons from Macintosh disks
 * Copyright (C) 2009 Jesus A. Alvarez
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#import "DSKIconFactory.h"
#import <libhfs/hfs.h>
#import <libmfs/mfs.h>
#import <libres/res.h>
#if defined(BUILD_MACOSX)
#import <AppKit/AppKit.h>
#elif defined(BUILD_IPHONE)
#import <UIKit/UIKit.h>
NSData *UIImageJPEGRepresentation(UIImage *image, CGFloat compressionQuality);
NSData *UIImagePNGRepresentation(UIImage *image);
#endif
#import "DSKIconFactory-Private.h"

extern BOOL quiet;

static DSKIconFactory *sharedInstance;
// Mac OS 1 bit palette
static uint32_t ctb1[2] = {0xFFFFFF, 0x000000};

// Mac OS 4 bit palette
static uint32_t ctb4[16] = {
    0xFFFFFF, 0xFFFF00, 0xFF6600, 0xDD0000, 0xFF0099, 0x330099, 0x0000DD, 0x0099FF,
    0x00BB00, 0x006600, 0x663300, 0x996633, 0xCCCCCC, 0x888888, 0x444444, 0x000000
};
// Mac OS 8 bit palette
static uint32_t ctb8[256] = {
    0xFFFFFF, 0xFFFFCC, 0xFFFF99, 0xFFFF66, 0xFFFF33, 0xFFFF00, 0xFFCCFF, 0xFFCCCC,
    0xFFCC99, 0xFFCC66, 0xFFCC33, 0xFFCC00, 0xFF99FF, 0xFF99CC, 0xFF9999, 0xFF9966,
    0xFF9933, 0xFF9900, 0xFF66FF, 0xFF66CC, 0xFF6699, 0xFF6666, 0xFF6633, 0xFF6600,
    0xFF33FF, 0xFF33CC, 0xFF3399, 0xFF3366, 0xFF3333, 0xFF3300, 0xFF00FF, 0xFF00CC,
    0xFF0099, 0xFF0066, 0xFF0033, 0xFF0000, 0xCCFFFF, 0xCCFFCC, 0xCCFF99, 0xCCFF66,
    0xCCFF33, 0xCCFF00, 0xCCCCFF, 0xCCCCCC, 0xCCCC99, 0xCCCC66, 0xCCCC33, 0xCCCC00,
    0xCC99FF, 0xCC99CC, 0xCC9999, 0xCC9966, 0xCC9933, 0xCC9900, 0xCC66FF, 0xCC66CC,
    0xCC6699, 0xCC6666, 0xCC6633, 0xCC6600, 0xCC33FF, 0xCC33CC, 0xCC3399, 0xCC3366,
    0xCC3333, 0xCC3300, 0xCC00FF, 0xCC00CC, 0xCC0099, 0xCC0066, 0xCC0033, 0xCC0000,
    0x99FFFF, 0x99FFCC, 0x99FF99, 0x99FF66, 0x99FF33, 0x99FF00, 0x99CCFF, 0x99CCCC,
    0x99CC99, 0x99CC66, 0x99CC33, 0x99CC00, 0x9999FF, 0x9999CC, 0x999999, 0x999966,
    0x999933, 0x999900, 0x9966FF, 0x9966CC, 0x996699, 0x996666, 0x996633, 0x996600,
    0x9933FF, 0x9933CC, 0x993399, 0x993366, 0x993333, 0x993300, 0x9900FF, 0x9900CC,
    0x990099, 0x990066, 0x990033, 0x990000, 0x66FFFF, 0x66FFCC, 0x66FF99, 0x66FF66,
    0x66FF33, 0x66FF00, 0x66CCFF, 0x66CCCC, 0x66CC99, 0x66CC66, 0x66CC33, 0x66CC00,
    0x6699FF, 0x6699CC, 0x669999, 0x669966, 0x669933, 0x669900, 0x6666FF, 0x6666CC,
    0x666699, 0x666666, 0x666633, 0x666600, 0x6633FF, 0x6633CC, 0x663399, 0x663366,
    0x663333, 0x663300, 0x6600FF, 0x6600CC, 0x660099, 0x660066, 0x660033, 0x660000,
    0x33FFFF, 0x33FFCC, 0x33FF99, 0x33FF66, 0x33FF33, 0x33FF00, 0x33CCFF, 0x33CCCC,
    0x33CC99, 0x33CC66, 0x33CC33, 0x33CC00, 0x3399FF, 0x3399CC, 0x339999, 0x339966,
    0x339933, 0x339900, 0x3366FF, 0x3366CC, 0x336699, 0x336666, 0x336633, 0x336600,
    0x3333FF, 0x3333CC, 0x333399, 0x333366, 0x333333, 0x333300, 0x3300FF, 0x3300CC,
    0x330099, 0x330066, 0x330033, 0x330000, 0x00FFFF, 0x00FFCC, 0x00FF99, 0x00FF66,
    0x00FF33, 0x00FF00, 0x00CCFF, 0x00CCCC, 0x00CC99, 0x00CC66, 0x00CC33, 0x00CC00,
    0x0099FF, 0x0099CC, 0x009999, 0x009966, 0x009933, 0x009900, 0x0066FF, 0x0066CC,
    0x006699, 0x006666, 0x006633, 0x006600, 0x0033FF, 0x0033CC, 0x003399, 0x003366,
    0x003333, 0x003300, 0x0000FF, 0x0000CC, 0x000099, 0x000066, 0x000033, 0xEE0000,
    0xDD0000, 0xBB0000, 0xAA0000, 0x880000, 0x770000, 0x550000, 0x440000, 0x220000,
    0x110000, 0x00EE00, 0x00DD00, 0x00BB00, 0x00AA00, 0x008800, 0x007700, 0x005500,
    0x004400, 0x002200, 0x001100, 0x0000EE, 0x0000DD, 0x0000BB, 0x0000AA, 0x000088,
    0x000077, 0x000055, 0x000044, 0x000022, 0x000011, 0xEEEEEE, 0xDDDDDD, 0xBBBBBB,
    0xAAAAAA, 0x888888, 0x777777, 0x555555, 0x444444, 0x222222, 0x111111, 0x000000
};

const char _pngIconDCAS[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 
    0x08, 0x06, 0x00, 0x00, 0x00, 0x73, 0x7A, 0x7A, 0xF4, 0x00, 0x00, 0x00, 
    0x01, 0x73, 0x52, 0x47, 0x42, 0x00, 0xAE, 0xCE, 0x1C, 0xE9, 0x00, 0x00, 
    0x01, 0x2D, 0x49, 0x44, 0x41, 0x54, 0x58, 0xC3, 0xDD, 0x57, 0xED, 0x0E, 
    0x83, 0x20, 0x0C, 0xF4, 0x08, 0xEF, 0xFF, 0xCA, 0xEC, 0x8F, 0xB8, 0xB3, 
    0xD2, 0x0F, 0x0B, 0x73, 0xCB, 0x9A, 0x2C, 0x68, 0x06, 0xA5, 0xDC, 0xF5, 
    0x6A, 0xC1, 0x46, 0xD6, 0x5A, 0x6B, 0xFD, 0x19, 0x00, 0xE4, 0xFB, 0x68, 
    0x9E, 0x66, 0x3C, 0xDF, 0xB2, 0xC2, 0x4E, 0x01, 0x6C, 0xFD, 0xD7, 0x37, 
    0x91, 0xEF, 0xB4, 0x81, 0x39, 0x46, 0x0D, 0xBC, 0xB9, 0x67, 0xAD, 0xB5, 
    0x03, 0x19, 0x6B, 0x7E, 0x9F, 0x17, 0x46, 0x60, 0x77, 0x7A, 0x2C, 0xD6, 
    0x9C, 0x7E, 0xC2, 0xAA, 0xE4, 0x5C, 0x72, 0xAF, 0x71, 0xBA, 0x9F, 0x72, 
    0x3A, 0x50, 0x64, 0x23, 0xD7, 0x68, 0x60, 0x9A, 0x22, 0x89, 0x5A, 0x47, 
    0x8E, 0x57, 0xC1, 0xBB, 0x27, 0xAF, 0x3A, 0x9E, 0x54, 0x70, 0x27, 0xC3, 
    0x49, 0x19, 0x2E, 0xEC, 0x9E, 0x4A, 0x4A, 0x76, 0xA1, 0xF5, 0xDF, 0x1D, 
    0x14, 0xA1, 0x51, 0x70, 0x57, 0xCF, 0x5A, 0x32, 0x5A, 0x79, 0x32, 0x44, 
    0x20, 0xA2, 0x5F, 0x09, 0xFD, 0x68, 0xEC, 0x34, 0xA5, 0x55, 0x30, 0xCA, 
    0x72, 0x4D, 0x7A, 0x81, 0xE2, 0x15, 0x47, 0xE0, 0x69, 0x2B, 0xB3, 0x1C, 
    0xCF, 0x16, 0xA2, 0xBA, 0x5A, 0xFB, 0x69, 0x04, 0x34, 0xD9, 0x45, 0xB4, 
    0x3E, 0x55, 0xAC, 0x22, 0x5F, 0xC3, 0x4C, 0xF2, 0x79, 0xD4, 0xF4, 0x24, 
    0xAC, 0x2B, 0x4E, 0x11, 0x0D, 0x70, 0x24, 0x71, 0x6E, 0x48, 0x86, 0x30, 
    0x7B, 0xCE, 0x65, 0x6D, 0x4F, 0xE5, 0x00, 0x76, 0xB3, 0x4A, 0xF2, 0xD7, 
    0x64, 0x68, 0x9D, 0x8C, 0xD1, 0xC9, 0xA2, 0x50, 0x02, 0xBC, 0xFD, 0x2E, 
    0x02, 0x8F, 0x04, 0xF0, 0xF7, 0x08, 0x54, 0xAF, 0xC7, 0x9B, 0x41, 0x80, 
    0x83, 0xEF, 0x77, 0x0B, 0xA9, 0xB6, 0x12, 0x59, 0x9C, 0x29, 0xC7, 0xDC, 
    0x13, 0xF0, 0x9E, 0x97, 0x0B, 0x8E, 0x87, 0x40, 0xA6, 0xEA, 0x19, 0x3D, 
    0x00, 0x77, 0x4A, 0xEF, 0x7E, 0x80, 0xA3, 0xF2, 0x4E, 0x29, 0x4E, 0x73, 
    0x19, 0xB5, 0x00, 0xB9, 0x23, 0x3E, 0x5D, 0xFD, 0x32, 0x3D, 0xE0, 0xAA, 
    0x8F, 0x13, 0x00, 0x94, 0xD4, 0x85, 0x72, 0xA1, 0x34, 0xEB, 0x13, 0x52, 
    0xB3, 0xEC, 0x05, 0x08, 0x13, 0x15, 0x65, 0x68, 0xA6, 0xF1, 0x57, 0x00, 
    0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
};

@implementation DSKIconFactory

+ (void)initialize {
    if (self != [DSKIconFactory class]) return;
    sharedInstance = [[DSKIconFactory alloc] init];
}

+ (DSKIconFactory*)sharedInstance {
    return sharedInstance;
}

- (NSData*)iconForDiskImage:(NSString*)path {
    // determine format and offset of disk image
    NSFileHandle * fh = [NSFileHandle fileHandleForReadingAtPath:path];
    if (fh == nil) return nil;
    [fh seekToFileOffset:1024];
    NSData * checkHeader = [fh readDataOfLength:128];
    [fh closeFile];
    const unsigned char * chb = [checkHeader bytes];
    
    // determine type from header
    if ((chb[0] == 0x42) && (chb[1] == 0x44)) {
        /* hfs */
        return [self iconForHFSDiskImage:path withOffset:0];
    } else if ((chb[0] == 0xD2) && (chb[1] == 0xD7)) {
        /* mfs */
        return [self iconForMFSDiskImage:path withOffset:0];
    } else if ((chb[84] == 0x42) && (chb[85] == 0x44)) {
        /* hfs, old style disk image header */
        return [self iconForHFSDiskImage:path withOffset:84];
    } else if ((chb[84] == 0xD2) && (chb[85] == 0xD7)) {
        /* mfs, old style disk image header */
        return [self iconForMFSDiskImage:path withOffset:84];
    }
    
    return nil;
}

#if 0
#pragma mark -
#pragma mark MFS
#endif

- (NSData*)iconForMFSDiskImage:(NSString*)path withOffset:(long)offset {
    // open disk image
    MFSVolume *vol = mfs_vopen([path fileSystemRepresentation], (size_t)offset, 0);
    if (vol == NULL) {
        NSLog(@"Can't open MFS volume at %@", path);
        return nil;
    }
    NSString * volName = [NSString stringWithCString:vol->name encoding:NSMacOSRomanStringEncoding];
    NSString * volComment;
    char * const volCommentBytes = mfs_comment(vol, NULL);
    if (volCommentBytes) {
         volComment = [NSString stringWithCString:volCommentBytes encoding:NSMacOSRomanStringEncoding];
         free(volCommentBytes);
    }
    
    // find applications
    MFSDirectoryRecord *rec;
    NSMutableArray *apps = [NSMutableArray arrayWithCapacity:5];
    for(int i=0; vol->directory[i]; i++) {
        rec = vol->directory[i];
        if (ntohl(rec->flUsrWds.type) != 'APPL') continue;
        [apps addObject:[NSNumber numberWithInt:i]];
    }
    
    // if there's more than one app, find one that looks matching
    if ([apps count] == 0) return nil;
    if ([apps count] > 1) {
        rec = NULL;
        for(NSNumber * num in apps) {
            rec = vol->directory[[num intValue]];
            NSString * appName = [[NSString alloc] initWithCString:rec->flCName encoding:NSMacOSRomanStringEncoding];
            if (![self chooseApp:appName inVolume:volName hint:volComment]) rec = NULL;
            [appName release];
            if (rec) break;
        }
        if (rec == NULL) return nil;
    } else {
        rec = vol->directory[[[apps objectAtIndex:0] intValue]];
    }
    
    // open resource fork
    MFSFork * rsrcFork = mfs_fkopen(vol, rec, kMFSForkRsrc, 0);
    RFILE * rfile = res_open_funcs(rsrcFork, mfs_fkseek, mfs_fkread);
    
    // get icon
    CGImageRef iconImage = [self appIconForResourceFile:rfile creator:ntohl(rec->flUsrWds.creator)];
    NSData * iconData = nil;
    if (iconImage) {
        iconData = [self imageDataFromCGImage:iconImage];
        CGImageRelease(iconImage);
    }
    
    // close stuff
    res_close(rfile);
    mfs_fkclose(rsrcFork);
    mfs_vclose(vol);
    
    return iconData;
}

#if 0
#pragma mark -
#pragma mark HFS
#endif

- (NSData*)iconForHFSDiskImage:(NSString*)path withOffset:(long)offset {
    // open disk image
    int mountFlags = HFS_MODE_RDONLY;
    if (offset == 84) mountFlags |= HFS_OPT_OLDHEADER;
    hfsvol * vol = hfs_mount([path fileSystemRepresentation], 0, mountFlags);
    if (vol == NULL) {
        NSLog(@"Can't open HFS volume at %@ with flags %x", path, mountFlags);
        return nil;
    }
    
    // try volume icon
    NSData * iconData = nil;
    iconData = [self iconFromHFSVolumeIcon:vol];
    if (iconData) {
        hfs_umount(vol);
        return iconData;
    }
    
    // find best application
    NSString * appPath = [self findAppInHFSVolume:vol];
    hfsfile * hfile = NULL;
    RFILE * rfile = NULL;
    if (appPath == nil) return nil;
    if (!quiet) NSLog(@"Using icon for %@", appPath);
    
    // open resource fork
    hfile = hfs_open(vol, [appPath cStringUsingEncoding:NSMacOSRomanStringEncoding]);
    if (hfile == NULL) goto end;
    hfs_setfork(hfile, 1);
    rfile = res_open_funcs(hfile, (res_seek_func)hfs_seek, (res_read_func)hfs_read);
    if (rfile == NULL) goto end;
    
    // get icon
    hfsdirent ent;
    if (hfs_stat(vol, [appPath cStringUsingEncoding:NSMacOSRomanStringEncoding], &ent)) goto end;
    CGImageRef iconImage = [self appIconForResourceFile:rfile creator:ntohl(*(uint32_t*)ent.u.file.creator)];
    if (iconImage) {
        iconData = [self imageDataFromCGImage:iconImage];
        CGImageRelease(iconImage);
    }
    
end:
    // close stuff
    if (rfile) res_close(rfile);
    if (hfile) hfs_close(hfile);
    hfs_umount(vol);
    
    return iconData;
}

- (NSString *)findAppInHFSVolume:(hfsvol*)vol {
    // get disk name
    hfsvolent volEnt;
    hfs_vstat(vol, &volEnt);
    NSString * volName = [NSString stringWithCString:volEnt.name encoding:NSMacOSRomanStringEncoding];
    NSString * volComment = [self commentForHFSVolume:vol];
    
    // find apps
    NSMutableArray * apps = [[NSMutableArray alloc] initWithCapacity:5];
    [self findApps:apps inDirectory:HFS_CNID_ROOTDIR ofHFSVolume:vol skipFolder:volEnt.blessed];
    
    // decide which one to use
    NSString * myApp = nil;
    NSString * appName = nil;
    NSCharacterSet * hfsPathSeparator = [NSCharacterSet characterSetWithCharactersInString:@":"];
    if ([apps count] == 1) myApp = [apps objectAtIndex:0];
    if ([apps count] > 1) for(NSString * appPath in apps) {
        // choose an app
        NSUInteger nameIndex = [appPath rangeOfCharacterFromSet:hfsPathSeparator options:NSBackwardsSearch].location;
        if (nameIndex == NSNotFound) continue;
        appName = [appPath substringFromIndex:nameIndex+1];
        if (![self chooseApp:appName inVolume:volName hint:volComment]) continue;
        myApp = appPath;
    }
    
    [myApp retain];
    [apps release];
    return [myApp autorelease];
}

- (void)findApps:(NSMutableArray*)apps inDirectory:(unsigned long)cnid ofHFSVolume:(hfsvol*)vol skipFolder:(unsigned long)skipCNID {
    if (hfs_setcwd(vol, cnid)) return;
    hfsdir * dir = hfs_opendir(vol, ":");
    if (dir == NULL) return;
    hfsdirent ent;
    while (hfs_readdir(dir, &ent) == 0) {
        if (ent.flags & HFS_ISDIR && ent.cnid != skipCNID) {
            [self findApps:apps inDirectory:ent.cnid ofHFSVolume:vol skipFolder:skipCNID];
        } else if (ntohl(*(uint32_t*)ent.u.file.type) == 'APPL') {
            // Found an app
            [apps addObject:[self pathToDirEntry:&ent ofHFSVolume:vol]];
        }
    }
    hfs_closedir(dir);
}

- (NSString*)pathToDirEntry:(const hfsdirent*)ent ofHFSVolume:(hfsvol*)vol {
    NSMutableString * path = [NSMutableString stringWithCString:ent->name encoding:NSMacOSRomanStringEncoding];
    NSString * entName;
    char name[HFS_MAX_FLEN + 1];
    unsigned long cnid= ent->parid;
    while (cnid != HFS_CNID_ROOTPAR) {
        if (hfs_dirinfo(vol, &cnid, name)) return nil;
        entName = [[NSString alloc] initWithCString:name encoding:NSMacOSRomanStringEncoding];
        [path insertString:@":" atIndex:0];
        [path insertString:entName atIndex:0];
        [entName release];
    }
    return path;
}

- (NSString *)commentForHFSVolume:(hfsvol*)vol {
    hfsvolent vent;
    hfsdirent dent;
    NSString * comment = nil;
    
    // get comment ID
    if (hfs_vstat(vol, &vent)) return nil;
    if (hfs_stat(vol, ":", &dent)) return nil;
    unsigned short cmtID = dent.fdcomment;
    //NSLog(@"HFS Comment ID %hd", cmtID);
    
    // open desktop
    hfsfile * hfile = NULL;
    RFILE * rfile = NULL;
    hfs_chdir(vol, vent.name);
    hfile = hfs_open(vol, "Desktop");
    if (hfile == NULL) {
        if (!quiet) NSLog(@"Desktop file not found, new version not supported");
        goto end;
    }
    // TODO support Desktop DB format, but it's not documented
    hfs_setfork(hfile, 1);
    rfile = res_open_funcs(hfile, (res_seek_func)hfs_seek, (res_read_func)hfs_read);
    if (rfile == NULL) goto end;
    
    // read resource
    unsigned char cmtLen;
    size_t readBytes;
    res_read(rfile, 'FCMT', cmtID, &cmtLen, 0, 1, &readBytes, NULL);
    if (readBytes == 0) goto end;
    char cmtBytes[256];
    res_read(rfile, 'FCMT', cmtID, cmtBytes, 1, cmtLen, &readBytes, NULL);
    cmtBytes[cmtLen] = '\0';
    comment = [NSString stringWithCString:cmtBytes encoding:NSMacOSRomanStringEncoding];
    
    // close
end:
    if (rfile) res_close(rfile);
    if (hfile) hfs_close(hfile);
    return comment;
}

- (NSData*)iconFromHFSVolumeIcon:(hfsvol*)vol {
    NSData * iconData = nil;
    hfsvolent vent;
    hfsdirent dent;
    if (hfs_vstat(vol, &vent)) return nil;
    
    // open icon file
    hfs_chdir(vol, vent.name);
    hfsfile * hfile = NULL;
    RFILE * rfile = NULL;
    hfile = hfs_open(vol, "Icon\x0D");
    if (hfile == NULL) goto end;
    hfs_setfork(hfile, 1);
    rfile = res_open_funcs(hfile, (res_seek_func)hfs_seek, (res_read_func)hfs_read);
    if (rfile == NULL) goto end;
    
    // read icon family
    NSDictionary * iconFamily = [self iconFamilyID:-16455 inResourceFile:rfile];
    
    // create image
    CGImageRef iconImage = [self iconImageFromFamily:iconFamily];
    if (iconImage) {
        iconData = [self imageDataFromCGImage:iconImage];
        CGImageRelease(iconImage);
    }
    
end:
    if (rfile) res_close(rfile);
    if (hfile) hfs_close(hfile);
    return iconData;
}

#if 0
#pragma mark -
#pragma mark App Choosing
#endif

- (BOOL)chooseApp:(NSString*)appName inVolume:(NSString*)volName hint:(NSString*)hint {
    return ([appName hasPrefix:volName] || 
            [volName hasPrefix:appName] ||
            [volName isEqualToString:appName] ||
            [appName isEqualToString:hint]);
}

#if 0
#pragma mark -
#pragma mark Resource Access
#endif

- (CGImageRef)appIconForResourceFile:(RFILE*)rfile creator:(OSType)creator {
    // special case
    CGDataProviderRef specialSource = NULL;
    switch(creator) {
        case 'DCAS':
            specialSource = CGDataProviderCreateWithData(NULL, _pngIconDCAS, sizeof _pngIconDCAS, NULL);
            break;
    }
    if (specialSource) {
        CGImageRef img = CGImageCreateWithPNGDataProvider(specialSource, NULL, false, kCGRenderingIntentDefault);
        CGDataProviderRelease(specialSource);
        return img;
    }
    
    // load bundle
    size_t numBundles;
    ResAttr * bundles = res_list(rfile, 'BNDL', NULL, 0, 0, &numBundles, NULL);
    void * bundle = NULL;
    if (numBundles == 0) return nil;
    for(int i=0; i < numBundles; i++) {
        bundle = res_read(rfile, 'BNDL', bundles[i].ID, NULL, 0, 0, NULL, NULL);
        if (bundle == NULL) break;
        if (ntohl(*(OSType*)bundle) == creator) break;
        free(bundle);
        bundle = NULL;
    }
    free(bundles);
    if (bundle == NULL) {
        if (!quiet) NSLog(@"BNDL resource not found");
        return nil;
    }
    
    // read bundle
    int iconID = [self iconFamilyIDForType:'APPL' inBundle:bundle inResourceFile:rfile];
    free(bundle);
    if (iconID == NSNotFound) return nil;
    
    // read icon family
    NSDictionary * iconFamily = [self iconFamilyID:iconID inResourceFile:rfile];
    
    // create image
    return [self iconImageFromFamily:iconFamily];
}

- (NSDictionary*)iconFamilyID:(int16_t)famID inResourceFile:(RFILE*)rfile {
    NSMutableDictionary * iconFamily = [NSMutableDictionary dictionaryWithCapacity:6];
    NSData * icnsData, * iconData, * maskData;
    void * iconRsrc;
    size_t resSize;
    
    if (0 /*iconRsrc = res_read(rfile, 'icns', famID, NULL, 0, 0, &resSize, NULL)*/) {
        // single icns resource
        icnsData = [NSData dataWithBytes:iconRsrc length:resSize];
        free(iconRsrc);
        
    } else {
        // separate resources
        const uint32_t iconResourceTypes[] = {'ICN#', 'icl4', 'icl8', 'ics#', 'ics4', 'ics8', 0};
        for(int i=0; iconResourceTypes[i]; i++) {
            iconRsrc = res_read(rfile, iconResourceTypes[i], famID, NULL, 0, 0, &resSize, NULL);
            if (iconRsrc == NULL) continue;
            [iconFamily setObject:[NSData dataWithBytes:iconRsrc length:resSize] forKey:[NSString stringWithFormat:@"%c%c%c%c", TYPECHARS(iconResourceTypes[i])]];
            free(iconRsrc);
        }
    }
    
    // mask pseudo-resources
    if (iconData = [iconFamily objectForKey:@"ICN#"]) {
        maskData = [iconData subdataWithRange:NSMakeRange(0x80, 0x80)];
        [iconFamily setObject:maskData forKey:@"IMK#"];
    }
    if (iconData = [iconFamily objectForKey:@"ics#"]) {
        maskData = [iconData subdataWithRange:NSMakeRange(0x20, 0x20)];
        [iconFamily setObject:maskData forKey:@"imk#"];
    }
    
    return iconFamily;
}

- (int)iconFamilyIDForType:(OSType)type inBundle:(void*)bndl inResourceFile:(RFILE*)rfile {
    short numIconFamilies = RSHORT(bndl, 0x0C) + 1;
    short * iconFamily = (short*)(bndl + 0x0E);
    short numFileRefs = RSHORT(bndl, (numIconFamilies*4) + 0x12) + 1;
    short * fileRef = (short*)(bndl + (numIconFamilies*4) + 0x14);
    
    // find FREF for APPL type
    short localIconID;
    void * FREF = NULL;
    for(int i=0; i < 2*numFileRefs; i+=2) {
        FREF = res_read(rfile, 'FREF', (int)ntohs(fileRef[i+1]), NULL, 0, 0, NULL, NULL);
        if (FREF == NULL) break;
        if (RLONG(FREF, 0) == 'APPL') break;
        free(FREF);
        FREF = NULL;
    }
    if (FREF == NULL) {
        if (!quiet) NSLog(@"FREF resource not found");
        return NSNotFound;
    }
    
    // read FREF
    localIconID = RSHORT(FREF, 4);
    free(FREF);
    
    // find resource ID for local ID
    for(int i=0; i < 2*numIconFamilies; i+=2) {
        if (ntohs(iconFamily[i]) == localIconID) return (int)ntohs(iconFamily[i+1]);
    }
    
    return NSNotFound;
}

- (CGImageRef)iconImageFromFamily:(NSDictionary*)iconFamily {
    NSData * iconData, *iconMask;
    if (iconMask = [iconFamily objectForKey:@"IMK#"]) {
        // has large mask, find best large icon
        if (iconData = [iconFamily objectForKey:@"icl8"])
            return [self iconImageWithData:iconData mask:iconMask size:32 depth:8];
        else if (iconData = [iconFamily objectForKey:@"icl4"])
            return [self iconImageWithData:iconData mask:iconMask size:32 depth:4];
        else iconData = [iconFamily objectForKey:@"ICN#"];
        return [self iconImageWithData:iconData mask:iconMask size:32 depth:1];
    } else if (iconMask = [iconFamily objectForKey:@"imk#"]) {
        // has small mask, find best small icon
        if (iconData = [iconFamily objectForKey:@"ics8"])
            return [self iconImageWithData:iconData mask:iconMask size:32 depth:8];
        else if (iconData = [iconFamily objectForKey:@"ics4"])
            return [self iconImageWithData:iconData mask:iconMask size:32 depth:4];
        else iconData = [iconFamily objectForKey:@"ics#"];
        return [self iconImageWithData:iconData mask:iconMask size:32 depth:1];
    }
    return NULL;
}

- (CGImageRef)iconImageWithData:(NSData*)iconData mask:(NSData*)iconMask size:(int)size depth:(int)depth {
    if (iconData == nil || iconMask == nil) return NULL;
    
    // convert to ARGB
    #define _iSETPIXELRGB(px,py,sa,srgb) data[(4*(px+(py*size)))+0] = sa;\
        data[(4*(px+(py*size)))+1] = ((srgb >> 16) & 0xFF);\
        data[(4*(px+(py*size)))+2] = ((srgb >> 8) & 0xFF);\
        data[(4*(px+(py*size)))+3] = (srgb & 0xFF)
    
    CFMutableDataRef pixels = CFDataCreateMutable(kCFAllocatorDefault, 4 * size * size);
    CFDataSetLength(pixels, 4 * size * size);
    unsigned char * data = CFDataGetMutableBytePtr(pixels);
    const unsigned char * pixelData = [iconData bytes];
    const unsigned char * maskData = [iconMask bytes];
    int m, mxy, pxy, rgb;
    if (pixels == NULL) return NULL;
    switch(depth) {
    case 1:
        // 1-bit
        for(int y = 0; y < size; y++) for(int x = 0; x < size; x++) {
            mxy = pxy = (y*(size/8)) + (x/8);
            m = ((maskData[mxy] >> (7-(x%8))) & 0x01)?0xFF:0x00;
            rgb = ctb1[((pixelData[pxy] >> (7-(x%8))) & 0x01)];
            _iSETPIXELRGB(x, y, m, rgb);
        }
        break;
    case 4:
        // 4-bit
        for(int y = 0; y < size; y++) for(int x = 0; x < size; x++) {
            mxy = (y*(size/8)) + (x/8);
            pxy = (y*(size/2)) + (x/2);
            m = ((maskData[mxy] >> (7-(x%8))) & 0x01)?0xFF:0x00;
            rgb = ctb4[(pixelData[pxy] >> 4*(1-x%2)) & 0x0F];
            _iSETPIXELRGB(x, y, m, rgb);
        }
        break;
    case 8:
        // 8-bit
        for(int y = 0; y < size; y++) for(int x = 0; x < size; x++) {
            mxy = (y*(size/8)) + (x/8);
            pxy = (y*size) + x;
            m = ((maskData[mxy] >> (7-(x%8))) & 0x01)?0xFF:0x00;
            rgb = ctb8[pixelData[pxy]];
            _iSETPIXELRGB(x, y, m, rgb);
        }
        break;
    }
    
    // create image
    CGDataProviderRef provider = CGDataProviderCreateWithCFData(pixels);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGImageRef image = CGImageCreate(size, size, 8, 32, size * 4, colorSpace, kCGImageAlphaFirst | kCGBitmapByteOrder32Big, provider, NULL, false, kCGRenderingIntentDefault);
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(colorSpace);
    CFRelease(pixels);
    return image;
}

- (NSData*)imageDataFromCGImage:(CGImageRef)img {
    NSData * imageData = nil;
#if defined(BUILD_MACOSX)
    NSBitmapImageRep * rep = [[NSBitmapImageRep alloc] initWithCGImage:img];
    imageData = [NSBitmapImageRep representationOfImageRepsInArray:[NSArray arrayWithObject:rep] usingType:NSPNGFileType properties:nil];
    [rep release];
#elif defined(BUILD_IPHONE)
    UIImage * image = [[UIImage alloc] initWithCGImage:img];
    imageData = UIImagePNGRepresentation(image);
    [image release];
#endif
    return imageData;
}

@end
