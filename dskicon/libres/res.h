/*
 * libres - library for reading Macintosh resource forks
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

#ifndef _RES_H_
#define _RES_H_

#include <stdint.h>
#include <stdlib.h>

#define TYPECHARS(t) ((t) >> 24) & 0xFF, ((t) >> 16) & 0xFF, ((t) >> 8) & 0xFF, (t) & 0xFF

// in-memory structures
typedef struct RFILE RFILE;

typedef union __attribute__ ((__packed__)) {
    struct {
        unsigned int compressed:1;
        unsigned int changed:1;
        unsigned int preload:1;
        unsigned int protected:1;
        unsigned int locked:1;
        unsigned int purgeable:1;
        unsigned int sysHeap:1;
        unsigned int sysRef:1;
    } fl;
    uint8_t b;
} RFlags;

/* this structure is only valid while the file is open */
struct ResAttr {
    int16_t     ID;
    RFlags      flags;
    uint32_t    size;
    const char* name; // owned by file
};
typedef struct ResAttr ResAttr;

typedef unsigned long (*res_seek_func)(void *, long, int);
typedef unsigned long (*res_read_func)(void *, void *, unsigned long);

RFILE* res_open (const char *path, int mode);
RFILE* res_open_mem (void *buf, size_t size, int copy);
RFILE* res_open_funcs (void *priv, res_seek_func seek, res_read_func read);
int res_close (RFILE *rp);

size_t res_typecount (RFILE *rp);
uint32_t* res_types (RFILE *rp, uint32_t *buf, size_t start, size_t size, size_t *read, size_t *remain);
size_t res_count (RFILE *rp, uint32_t type);

/**
    @param buf      list output or NULL
    @param start    start element or 0
    @param size     number of elements to list or 0
    @param read     returns number of resources read, if not NULL
    @param remain   number of resources left unread, if not NULL
    @returns        buf (filled), or newly allocated complete list
 */
ResAttr* res_list (RFILE *rp, uint32_t type, ResAttr *buf, size_t start, size_t size, size_t *read, size_t *remain);
ResAttr* res_attr (RFILE *rp, uint32_t type, int16_t ID, ResAttr *buf);
ResAttr* res_attr_named (RFILE *rp, uint32_t type, const char *name, ResAttr *buf);

/**
    @param buf      resource output or NULL
    @param size     size of buffer, ignored if buf is NULL
    @returns        buf (filled), or newly allocated resource
 */
void* res_read (RFILE *rp, uint32_t type, int16_t ID, void *buf, size_t start, size_t size, size_t *read, size_t *remain);
void* res_read_named (RFILE *rp, uint32_t type, const char *name, void *buf, size_t start, size_t size, size_t *read, size_t *remain);

void res_printdir (RFILE *rp);
void res_printattr (const ResAttr *attr, uint32_t type);
#endif /* _RES_H_ */
