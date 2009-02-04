/*
 * libmfs - library for reading Macintosh MFS volumes
 * Copyright (C) 2008-2009 Jesus A. Alvarez
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

#ifndef _MFS_H_
#define _MFS_H_

#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <time.h>
#ifdef USE_LIBRES
#include <libres/libres.h>
#define DESKTOP_TYPE RFILE*
#else
#define DESKTOP_TYPE void*
#endif
#include "appledouble.h"

#define kMFSBlockSize       512
#define kMFSSignature       0xD2D7
#define kMFSTimeDelta       2082844800
#define kMFSAlBkEmpty       0
#define kMFSAlBkLast        1
#define kMFSAlBkDir         0xFFF
#define kMFSFolderTrash     -3
#define kMFSFolderDesktop   -2
#define kMFSFolderTemplate  -1
#define kMFSFolderRoot      0

enum {
    kMFSForkData,
    kMFSForkRsrc,
    kMFSForkAppleDouble
};

struct __attribute__ ((__packed__)) MFSMasterDirectoryBlock {
    uint16_t    drSigWord;      // always 0xD2D7
    uint32_t    drCrDate;       // date and time of initialization
    uint32_t    drLsBkUp;       // date and time of last backup
    uint16_t    drAtrb;         // volume attributes
    uint16_t    drNmFls;        // number of files in directory
    uint16_t    drDirSt;        // first block of directory
    uint16_t    drBlLen;        // lenght of directory in blocks
    uint16_t    drNmAlBlks;     // number of allocation blocks on volume
    uint32_t    drAlBlkSiz;     // size of allocation blocks
    uint32_t    drClpSiz;       // number of bytes to allocate
    uint16_t    drAlBlSt;       // first allocation block in block map
    uint32_t    drNxtFNum;      // next unused file number
    uint16_t    drFreeBks;      // number of unused allocation blocks
    uint8_t     drVN[28];       // volume name (pascal string)
};
typedef struct MFSMasterDirectoryBlock MFSMasterDirectoryBlock;

// use ntohl/ntohs when accessing this structure, but don't change it
struct __attribute__ ((__packed__)) MFSFInfo {
    uint32_t    type;
    uint32_t    creator;
    struct __attribute__ ((__packed__)) {
        unsigned int    fIsOnDesk:1;
        unsigned int    fColor:3;
        unsigned int    _rsv1:1;
        unsigned int    fRequiresSwitchLaunch:1;
        unsigned int    fIsShared:1;
        unsigned int    fHasNoINITs:1;
        unsigned int    fHasBeenInited:1;
        unsigned int    fLetter:1;  // formerly 'changed', actually reserved
        unsigned int    fHasCustomIcon:1;
        unsigned int    fIsStationery:1;    // system 7 and later
        unsigned int    fNameLocked:1;
        unsigned int    fHasBundle:1;
        unsigned int    fIsInvisible:1;
        unsigned int    fIsAlias:1;         // system 7 and later
    } flags;
    struct __attribute__ ((__packed__)) {
        int16_t v;
        int16_t h;
    } loc;
    uint16_t    folder;
};
typedef struct MFSFInfo MFSFInfo;

struct __attribute__ ((__packed__)) MFSDirectoryRecord {
    uint8_t     flFlags;        // file flags
    uint8_t     flTyp;          // version number
    MFSFInfo    flUsrWds;       // information used by the Finder
    uint32_t    flFlNum;        // file number
    uint16_t    flStBlk;        // first allocation block of data fork
    uint32_t    flLgLen;        // logical EOF of data fork
    uint32_t    flPyLen;        // physical EOF of data fork
    uint16_t    flRStBlk;       // first allocation block of resource fork
    uint32_t    flRLgLen;       // logical EOF of resource fork
    uint32_t    flRPyLen;       // physical EOF of resource fork
    uint32_t    flCrDat;        // date and time of creation
    uint32_t    flMdDat;        // date and time of last modification
    uint8_t     flNam[1];       // file name (pascal string)
    char        flCName[];
};
typedef struct MFSDirectoryRecord MFSDirectoryRecord;

// Volume Allocation Block Map, first item is length, 2nd item is unused
typedef uint16_t* MFSVABM;

typedef uint8_t MFSBlock[kMFSBlockSize];

struct MFSVolume {
    FILE                    *fp;
    size_t                  offset;     // offset to start of volume (for mounting disk images with header)
    size_t                  alBkOff;    // offset to allocation block 0
    size_t                  openForks;  // number of open forks
    MFSMasterDirectoryBlock mdb;
    MFSVABM                 vabm;
    MFSDirectoryRecord      **directory;
    DESKTOP_TYPE            desktop;
    char                    name[28];
};
typedef struct MFSVolume MFSVolume;

#define kMFSForkSignature 0x1337D00D
struct MFSFork {
    uint32_t            _fkSgn;     // signature
    MFSVolume           *fkVol;     // parent volume
    MFSDirectoryRecord  *fkDrRec;   // directory record
    uint32_t            fkLgLen;    // fork length (bytes)
    uint16_t            fkNmBks;    // number of blocks
    int                 fkMode;     // mode (kMFSFork*)
    AppleDouble         *fkAppleDouble;
    unsigned long       fkOffset;   // mfs_fkseek, mfs_fkread
    uint16_t            fkAlMap[];  // allocation map
};
typedef struct MFSFork MFSFork;

#define kAppleDoubleHeaderLength        0x300
#define kAppleDoubleResourceForkOffset  kAppleDoubleHeaderLength
#define kAppleDoubleFileInfoOffset      0x70
#define kAppleDoubleFileInfoLength      0x10
#define kAppleDoubleFinderInfoOffset    0x80
#define kAppleDoubleFinderInfoLength    0x20
#define kAppleDoubleRealNameOffset      0xA0
#define kAppleDoubleCommentOffset       0x1A0

// open/close volume
MFSVolume* mfs_vopen (const char *path, size_t offset, int reserved);
int mfs_vclose (MFSVolume* vol);

// convert time
time_t mfs_time (uint32_t mfsDate);
struct timespec mfs_timespec (uint32_t mfsDate);

// print structures
int mfs_printmdb (MFSMasterDirectoryBlock *mdb);
int mfs_printrecord (MFSDirectoryRecord *rec);

// directory
MFSDirectoryRecord ** mfs_directory (MFSVolume *vol);
void mfs_directory_free (MFSDirectoryRecord ** dir);
MFSDirectoryRecord* mfs_directory_find_name (MFSDirectoryRecord **dir, const char *name);
char * mfs_comment (MFSVolume *vol, MFSDirectoryRecord *rec);

// fork mgmt
MFSFork* mfs_fkopen (MFSVolume *vol, MFSDirectoryRecord *rec, int mode, int flags);
int mfs_fkclose (MFSFork *fk);
int mfs_fkread_at (MFSFork *fk, size_t size, size_t offset, void *buf);

// for librsrc/libres compatibility
unsigned long mfs_fkread (void *fk, void *buf, unsigned long length);
unsigned long mfs_fkseek (void *fk, long offset, int whence);

#endif /* _MFS_H_ */
