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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>
#include "mfs.h"

// printable flags
#define BINFLG8(x) ((x)&0x80?'1':'0'),((x)&0x40?'1':'0'),((x)&0x20?'1':'0'),((x)&0x10?'1':'0'),((x)&0x08?'1':'0'),((x)&0x04?'1':'0'),((x)&0x02?'1':'0'),((x)&0x01?'1':'0')
#define BINFLG16(x) BINFLG8((x>>8)), BINFLG8(x)

// private functions
int mfs_blkread (MFSVolume *vol, size_t numBlocks, size_t offset, void *buf);
int mfs_albkread (MFSVolume *vol, size_t numBlocks, uint16_t start, void *buf);
int mfs_fkread_at_appledouble (MFSFork *fk, size_t size, size_t offset, void *buf);
int mfs_fkread_at_real (MFSFork *fk, size_t size, size_t offset, void *buf);
MFSVABM mfs_vabm (MFSVolume *vol);
MFSDirectoryRecord* mfs_directory_record (MFSDirectoryRecord *src, size_t size);
int16_t mfs_comment_id (const char *flCName);
int16_t mfs_folder_id (MFSDirectoryRecord *rec);
#ifdef USE_LIBRES
RFILE * mfs_load_desktop (MFSVolume *vol);
#endif

MFSVolume* mfs_vopen (const char *path, size_t offset, int write) {
    FILE* fp = fopen(path, "r");
    if (fp == NULL) return NULL;
    MFSVolume* vol = malloc(sizeof(MFSVolume));
    bzero(vol, sizeof(MFSVolume));
    vol->fp = fp;
    vol->offset = offset;
    vol->openForks = 0;
    
    // read MDB
    void* mdb_block = malloc(kMFSBlockSize);
    if (-1 == mfs_blkread(vol, 1, 2, mdb_block)) goto error;
    memcpy(&vol->mdb, mdb_block, sizeof(MFSMasterDirectoryBlock));
    free(mdb_block);
    // bring to host endianness
    MFSMasterDirectoryBlock *mdb = &vol->mdb;
    mdb->drSigWord  = ntohs(mdb->drSigWord);
    mdb->drCrDate   = ntohl(mdb->drCrDate);
    mdb->drLsBkUp   = ntohl(mdb->drLsBkUp);
    mdb->drAtrb     = ntohs(mdb->drAtrb);
    mdb->drNmFls    = ntohs(mdb->drNmFls);
    mdb->drDirSt    = ntohs(mdb->drDirSt);
    mdb->drBlLen    = ntohs(mdb->drBlLen);
    mdb->drNmAlBlks = ntohs(mdb->drNmAlBlks);
    mdb->drAlBlkSiz = ntohl(mdb->drAlBlkSiz);
    mdb->drClpSiz   = ntohl(mdb->drClpSiz);
    mdb->drAlBlSt   = ntohs(mdb->drAlBlSt);
    mdb->drNxtFNum  = ntohl(mdb->drNxtFNum);
    mdb->drFreeBks  = ntohs(mdb->drFreeBks);
    strncpy(vol->name, (char*)&mdb->drVN[1], mdb->drVN[0]);
    
    // check MDB
    if (mdb->drSigWord != kMFSSignature) goto error;
    #if defined(LIBMFS_VERBOSE)
    mfs_printmdb(mdb);
    #endif
    
    // read volume allocation block map
    vol->vabm = mfs_vabm(vol);
    vol->alBkOff = mdb->drAlBlSt*kMFSBlockSize - 2*mdb->drAlBlkSiz;
    
    // read directory
    vol->directory = mfs_directory(vol);
    
    return vol;
error:
    errno = EFTYPE;
    free(vol);
    return NULL;
}

int mfs_vclose (MFSVolume* vol) {
    if (vol->openForks) {
        errno = EBUSY;
        return -1;
    }
    mfs_directory_free(vol->directory);
    free(vol->vabm);
    fclose(vol->fp);
#ifdef USE_LIBRES
    res_close(vol->desktop);
#endif
    free(vol);
    return 0;
}

int mfs_blkread (MFSVolume *vol, size_t numBlocks, size_t offset, void *buf) {
    if (-1 == fseeko(vol->fp, vol->offset+(kMFSBlockSize*offset), SEEK_SET)) return -1;
    if (numBlocks != fread(buf, kMFSBlockSize, numBlocks, vol->fp)) return -1;
    return 0;
}

int mfs_albkread (MFSVolume *vol, size_t numBlocks, uint16_t start, void *buf) {
    if (-1 == fseeko(vol->fp, (vol->offset)+(vol->alBkOff)+(vol->mdb.drAlBlkSiz*start), SEEK_SET)) return -1;
    if (numBlocks != fread(buf, vol->mdb.drAlBlkSiz, numBlocks, vol->fp)) return -1;
    return 0;
}

time_t mfs_time (uint32_t mfsDate) {
    return mfsDate - kMFSTimeDelta;
}

struct timespec mfs_timespec (uint32_t mfsDate) {
    struct timespec ts;
    ts.tv_sec = mfsDate - kMFSTimeDelta;
    ts.tv_nsec = 0;
    return ts;
}

int mfs_printmdb (MFSMasterDirectoryBlock *mdb) {
    time_t t;
    printf("MASTER DIRECTORY BLOCK:\n");
    printf("  signature:  $%04X\n", mdb->drSigWord);
    t = mfs_time(mdb->drCrDate);
    printf("  creation:   %s", asctime(localtime(&t)));
    t = mfs_time(mdb->drLsBkUp);
    printf("  backup:     %s", asctime(localtime(&t)));
    printf("  attributes: %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n", BINFLG16(mdb->drAtrb));
    printf("  files:      %d\n", mdb->drNmFls);
    printf("  dir.start:  %d\n", mdb->drDirSt);
    printf("  dir.len:    %d\n", mdb->drBlLen);
    printf("  al.bks:     %d\n", mdb->drNmAlBlks);
    printf("  al.bksz:    %d\n", mdb->drAlBlkSiz);
    printf("  al.bytes:   %d\n", mdb->drClpSiz);
    printf("  al.first:   %d\n", mdb->drAlBlSt);
    printf("  fn.next:    %d\n", mdb->drNxtFNum);
    printf("  free:       %d\n", mdb->drFreeBks);
    char volName[28];
    strncpy(volName, (char*)&mdb->drVN[1], mdb->drVN[0]);
    printf("  name:       %s\n", volName);
    
}

int mfs_printrecord (MFSDirectoryRecord *rec) {
    printf("DIRECTORY RECORD:\n");
    printf("  name:     %s\n", rec->flCName);
    printf("  flags:    %c%c%c%c%c%c%c%c\n", BINFLG8(rec->flFlags));
    printf("  version:  %d\n", rec->flTyp);
    // finder flags
    printf("  inode:    %d\n", rec->flFlNum);
    printf("  data.blk: %d\n", rec->flStBlk);
    printf("  data.lgl: %d\n", rec->flLgLen);
    printf("  data.pyl: %d\n", rec->flPyLen);
    printf("  rsrc.blk: %d\n", rec->flRStBlk);
    printf("  rsrc.lgl: %d\n", rec->flRLgLen);
    printf("  rsrc.pyl: %d\n", rec->flRPyLen);
    time_t t = mfs_time(rec->flCrDat);
    printf("  created:  %s", asctime(localtime(&t)));
    t = mfs_time(rec->flMdDat);
    printf("  modified: %s", asctime(localtime(&t)));
}

MFSVABM mfs_vabm (MFSVolume *vol) {
    // VABM is 12-bit packed and comes after MDB
    // read blocks containing VABM
    MFSMasterDirectoryBlock *mdb = &vol->mdb;
    size_t vabm_size = (mdb->drNmAlBlks*3)/2;
    size_t vabm_span = vabm_size + sizeof(MFSMasterDirectoryBlock);
    size_t vabm_blks = vabm_span/kMFSBlockSize + (vabm_span%kMFSBlockSize?1:0);
    void* vabm_bits = malloc(vabm_blks*kMFSBlockSize);
    if (-1 == mfs_blkread(vol, vabm_blks, 2, vabm_bits)) return NULL;
    
    // parse VABM
    void* vabm_base = vabm_bits + sizeof(MFSMasterDirectoryBlock);
    MFSVABM vabm = malloc(sizeof(uint16_t)*(mdb->drNmAlBlks+2));
    vabm[0] = mdb->drNmAlBlks;
    vabm[1] = 0x1337;
    
    size_t offset;
    uint16_t val;
    for(int n=2; n < 2+mdb->drNmAlBlks; n++) {
        offset = ((n-2)*3)/2;
        val = ntohs(*(uint16_t*)(vabm_base+offset));
        if (n%2) vabm[n] = val & 0xFFF;
        else vabm[n] = (val & 0xFFF0) >> 4;
    }
    
    return vabm;
}

// read directory
MFSDirectoryRecord ** mfs_directory (MFSVolume *vol) {
    MFSMasterDirectoryBlock *mdb = &vol->mdb;
    MFSBlock *dir_blk = calloc(mdb->drBlLen, kMFSBlockSize);
    // array of pointers to records
    MFSDirectoryRecord ** dir = calloc(mdb->drNmFls+1, sizeof(MFSDirectoryRecord*));
    dir[mdb->drNmFls] = NULL;
    
    // read directory blocks
    mfs_blkread(vol, mdb->drBlLen, mdb->drDirSt, dir_blk);
    
    // parse
    MFSDirectoryRecord *rec;
    size_t block, rec_offset;
    size_t rec_count = 0, rec_size;
    for(block = 0; block < mdb->drBlLen; block++) {
        // read records in a block
        rec_offset = 0;
        for(;;) {
            rec = (MFSDirectoryRecord*)&dir_blk[block][rec_offset];
            rec_size = 51 + rec->flNam[0];
            if (rec->flFlags) {
                // record is used, copy it
                dir[rec_count++] = mfs_directory_record(rec, rec_size);
                rec_offset += rec_size;
                if (rec_offset%2) rec_offset++;
            } else break;
        }
        if (rec_count == mdb->drNmFls) break;
    }
    
    free(dir_blk);
    return dir;
}

void mfs_directory_free (MFSDirectoryRecord ** dir) {
    for(int i=0; dir[i]; i++) free(dir[i]);
    free(dir);
}

MFSDirectoryRecord* mfs_directory_record (MFSDirectoryRecord *src, size_t size) {
    MFSDirectoryRecord *rec = malloc(size+1);
    memcpy(rec, src, size);
    // null-terminate name
    ((uint8_t*)rec)[size] = '\0';
    
    // bring to host endianness
    rec->flFlNum  = ntohl(rec->flFlNum);
    rec->flStBlk  = ntohs(rec->flStBlk);
    rec->flLgLen  = ntohl(rec->flLgLen);
    rec->flPyLen  = ntohl(rec->flPyLen);
    rec->flRStBlk = ntohs(rec->flRStBlk);
    rec->flRLgLen = ntohl(rec->flRLgLen);
    rec->flRPyLen = ntohl(rec->flRPyLen);
    rec->flCrDat  = ntohl(rec->flCrDat);
    rec->flMdDat  = ntohl(rec->flMdDat);
    
    #if defined(LIBMFS_VERBOSE)
    mfs_printrecord(rec);
    #endif
    
    return rec;
}

MFSDirectoryRecord* mfs_directory_find_name (MFSDirectoryRecord **dir, const char *name) {
    MFSDirectoryRecord *rec;
    size_t namelen = strlen(name);
    
    for(int i=0; dir[i]; i++) {
        rec = dir[i];
        if (rec->flNam[0] != namelen) continue;
        if (strcasecmp(rec->flCName, name) == 0) return rec;
    }
    return NULL;
}

// http://developer.apple.com/technotes/tb/tb_06.html
// Comments are in Desktop's FCMT resources, as a Str255
int16_t mfs_comment_id (const char *flCName) {
    int16_t hash = 0;
    
    for(int i = 0; flCName[i]; i++) {
        hash ^= flCName[i];
        // ROR.W
        if (hash & 1) hash = (hash >> 1) | 0x8000;
        else hash = ((hash >> 1) & 0x7fff);
        if (hash > 0) hash = - hash;
    }
    return hash;
}

// returns newly allocated C-string in MacOSRoman encoding, or NULL if it fails
// pass rec as NULL for the disk's comment
char * mfs_comment (MFSVolume *vol, MFSDirectoryRecord *rec) {
    if (vol == NULL) return NULL;
#if defined(USE_LIBRES)
    int16_t cmtID = mfs_comment_id(rec? rec->flCName : vol->name);
    unsigned char cmtLen;
    size_t readBytes;
    res_read(mfs_load_desktop(vol), 'FCMT', cmtID, &cmtLen, 0, 1, &readBytes, NULL);
    if (readBytes == 0) return NULL;
    char * comment = malloc((int)cmtLen+1);
    res_read(mfs_load_desktop(vol), 'FCMT', cmtID, comment, 1, cmtLen, &readBytes, NULL);
    comment[cmtLen] = '\0';
    return comment;
#else
    return NULL;
#endif
}

MFSFork* mfs_fkopen (MFSVolume *vol, MFSDirectoryRecord *rec, int mode, int write) {
    if (vol == NULL || rec == NULL) {errno = ENOENT; return NULL;}
    int isResourceFork = ((mode == kMFSForkRsrc) || (mode == kMFSForkAppleDouble));
    // cannot open non-existant resource forks
    // non-existant data forks behave like empty files however
    if ((mode == kMFSForkRsrc) && (rec->flRStBlk == 0)) {errno = ENOENT; return NULL;}
    
    uint16_t fkNmBks = (isResourceFork?rec->flRPyLen:rec->flPyLen)/vol->mdb.drAlBlkSiz;
    MFSFork* fk = malloc(sizeof(MFSFork) + (sizeof(uint16_t)*(fkNmBks+1)));
    fk->_fkSgn  = 0;
    fk->fkVol   = vol;
    fk->fkDrRec = rec;
    fk->fkMode  = mode;
    fk->fkLgLen = (isResourceFork?rec->flRLgLen:rec->flLgLen);
    fk->fkNmBks = fkNmBks;
    fk->fkAppleDouble = NULL;
    fk->fkOffset = 0;
    
    // read allocation map
    if (fkNmBks) {
        fk->fkAlMap[0] = (isResourceFork?rec->flRStBlk:rec->flStBlk);
        uint16_t lastAlBk = fk->fkAlMap[0];
        for(int i=1; i < fk->fkNmBks; i++) {
            fk->fkAlMap[i] = vol->vabm[lastAlBk];
            lastAlBk = fk->fkAlMap[i];
        }
        fk->fkAlMap[fkNmBks] = 0;
        if (vol->vabm[lastAlBk] != kMFSAlBkLast) {
            fprintf(stderr, "Invalid allocation block map for %s\n", rec->flCName);
            errno = EFBIG;
            free(fk);
            return NULL;
        };
    }
    
    // construct AppleDouble header
    if (mode == kMFSForkAppleDouble) {
        AppleDouble *as = malloc(kAppleDoubleHeaderLength);
        fk->fkAppleDouble = as;
        bzero(as, kAppleDoubleHeaderLength);
        
        // header
        as->magic = htonl(kAppleDoubleMagic);
        as->version = htonl(kAppleDoubleVersion);
        memcpy(&as->filesystem, "Macintosh       ", 16);
        int e = 0;
        
        // resource fork
        if (fk->fkLgLen) {
            as->entry[e].type = htonl(kAppleDoubleResourceForkEntry);
            as->entry[e].offset = htonl(kAppleDoubleResourceForkOffset);
            as->entry[e].length = htonl((uint32_t)fk->fkLgLen);
            e++;
        }
        
        // real name
        as->entry[e].type = htonl(kAppleDoubleRealNameEntry);
        as->entry[e].offset = htonl(kAppleDoubleRealNameOffset);
        as->entry[e].length = htonl((uint32_t)(rec->flNam[0]));
        strcpy((void*)as+kAppleDoubleRealNameOffset, rec->flCName);
        e++;
        
        // file info
        as->entry[e].type = htonl(kAppleDoubleFileInfoEntry);
        as->entry[e].offset = htonl(kAppleDoubleFileInfoOffset);
        as->entry[e].length = htonl(kAppleDoubleFileInfoLength);
        AppleDoubleMacFileInfo *mfi = (void*)as+kAppleDoubleFileInfoOffset;
        mfi->creationDate = htonl(rec->flCrDat);
        mfi->modificationDate = htonl(rec->flMdDat);
        mfi->backupDate = htonl(0);
        mfi->attributes = htonl((uint32_t)(rec->flFlags & 0x7F));
        e++;
        
        // finder info
        as->entry[e].type = htonl(kAppleDoubleFinderInfoEntry);
        as->entry[e].offset = htonl(kAppleDoubleFinderInfoOffset);
        as->entry[e].length = htonl(kAppleDoubleFinderInfoLength);
        memcpy((void*)as+kAppleDoubleFinderInfoOffset, &rec->flUsrWds, 16);
        e++;
        
        // finder comment
#ifdef USE_LIBRES
        size_t commentLength;
        if (mfs_load_desktop(vol) && res_read(vol->desktop, 'FCMT', mfs_comment_id(rec->flCName), (void*)as+kAppleDoubleCommentOffset, 0, 256, &commentLength, NULL)) {
            as->entry[e].type = htonl(kAppleDoubleCommentEntry);
            as->entry[e].offset = htonl(kAppleDoubleCommentOffset);
            as->entry[e].length = htonl(commentLength);
            e++;
        }
#endif
        
        // number of entries written
        as->numEntries = htons(e);
    }
    
    // set signature and open forks
    vol->openForks++;
    fk->_fkSgn = kMFSForkSignature;
    return fk;
}

int mfs_fkclose (MFSFork *fk) {
    if (fk->_fkSgn != kMFSForkSignature) {
        errno = EBADF;
        return -1;
    }
    fk->_fkSgn = 0;
    if (fk->fkAppleDouble) free(fk->fkAppleDouble);
    fk->fkVol->openForks--;
    free(fk);
    return 0;
}

int mfs_fkread_at (MFSFork *fk, size_t size, size_t offset, void *buf) {
    switch(fk->fkMode) {
        case kMFSForkData:
        case kMFSForkRsrc:
            return mfs_fkread_at_real(fk, size, offset, buf);
        case kMFSForkAppleDouble:
            return mfs_fkread_at_appledouble(fk, size, offset, buf);
    }
}

unsigned long mfs_fkread (void *fk, void *buf, unsigned long length) {
    return mfs_fkread_at((MFSFork*)fk, length, ((MFSFork*)fk)->fkOffset, buf);
}

unsigned long mfs_fkseek (void *fk, long offset, int whence) {
    // set offset
    uint32_t fkLen = ((MFSFork*)fk)->fkLgLen + ((((MFSFork*)fk)->fkMode == kMFSForkAppleDouble)? kAppleDoubleHeaderLength : 0);
    
    switch(whence) {
        case SEEK_SET:
            ((MFSFork*)fk)->fkOffset = offset;
            break;
        case SEEK_END:
            ((MFSFork*)fk)->fkOffset = fkLen + offset;
            break;
        case SEEK_CUR:
            ((MFSFork*)fk)->fkOffset += offset;
            break;
    }
    return (unsigned long)((MFSFork*)fk)->fkOffset;
}

long mfs_ftell (MFSFork *fk) {
    return fk->fkOffset;
}

int mfs_fkread_at_appledouble (MFSFork *fk, size_t size, size_t offset, void *buf) {
    if (size == 0) return 0;
    size_t asLgLen = kAppleDoubleHeaderLength + fk->fkLgLen;
    if (offset >= asLgLen) return 0;
    if (offset + size > asLgLen) size = asLgLen - offset;
    
    // read in resource fork only
    if (offset >= kAppleDoubleResourceForkOffset)
        return mfs_fkread_at_real(fk, size, offset-kAppleDoubleResourceForkOffset, buf);
    
    // read from AppleDouble header:
    size_t btr = size;
    size_t hdBtr = kAppleDoubleHeaderLength - offset;
    if (hdBtr > size) hdBtr = size;
    memcpy(buf, fk->fkAppleDouble, hdBtr);
    btr -= hdBtr;
    buf += hdBtr;
    
    // read from fork
    if (btr) return hdBtr + mfs_fkread_at_real(fk, btr, 0, buf);
    return size;
}

int mfs_fkread_at_real (MFSFork *fk, size_t size, size_t offset, void *buf) {
    if (size == 0) return 0;
    if (offset >= fk->fkLgLen) return 0;
    if (offset + size > fk->fkLgLen) size = fk->fkLgLen - offset;
    
    // read blocks and copy data
    size_t btr = size;  // total bytes to read
    size_t bkBtr;       // bytes to read from block
    uint16_t bkn = offset / (fk->fkVol->mdb.drAlBlkSiz); // block index
    size_t bk1Off = offset % (fk->fkVol->mdb.drAlBlkSiz); // offset in first block
    void *bk = malloc(fk->fkVol->mdb.drAlBlkSiz);
    
    // read first block
    mfs_albkread(fk->fkVol, 1, fk->fkAlMap[bkn], bk);
    bkBtr = (fk->fkVol->mdb.drAlBlkSiz - bk1Off); // maximum bytes readable from first block
    if (bkBtr > btr) bkBtr = btr;
    memcpy(buf, bk+bk1Off, bkBtr);
    btr -= bkBtr;
    buf += bkBtr;
    bkn++;
    
    // read other blocks
    while(btr) {
        // read block
        mfs_albkread(fk->fkVol, 1, fk->fkAlMap[bkn], bk);
        // bytes to read
        if (btr >= fk->fkVol->mdb.drAlBlkSiz) bkBtr = fk->fkVol->mdb.drAlBlkSiz;
        else bkBtr = btr;
        // copy
        memcpy(buf, bk, bkBtr);
        // advance
        btr -= bkBtr;
        buf += bkBtr;
        bkn++;
    }
    
    free(bk);
    return (int)size;
}

#ifdef USE_LIBRES
RFILE * mfs_load_desktop (MFSVolume *vol) {
    if (vol->desktop == NULL) {
        MFSDirectoryRecord *dr = mfs_directory_find_name(vol->directory, "Desktop");
        MFSFork *df = mfs_fkopen(vol, dr, kMFSForkRsrc, 0);
        void* desktopData = malloc(df->fkLgLen);
        mfs_fkread_at(df, df->fkLgLen, 0, desktopData);
        vol->desktop = res_open_mem(desktopData, df->fkLgLen, 0);
        mfs_fkclose(df);
    }
    return vol->desktop;
}
#endif