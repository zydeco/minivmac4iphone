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

@interface DSKIconFactory ()
- (CGImageRef)appIconForResourceFile:(RFILE*)rfile creator:(OSType)creator;

- (NSString *)findAppInHFSVolume:(hfsvol*)vol;
- (void)findApps:(NSMutableArray*)apps inDirectory:(unsigned long)cnid ofHFSVolume:(hfsvol*)vol skipFolder:(unsigned long)skipCNID;
- (NSString*)pathToDirEntry:(const hfsdirent*)ent ofHFSVolume:(hfsvol*)vol;
- (NSString *)commentForHFSVolume:(hfsvol*)vol;
- (NSData*)iconFromHFSVolumeIcon:(hfsvol*)vol;

- (BOOL)chooseApp:(NSString*)appName inVolume:(NSString*)volName hint:(NSString*)hint;

- (NSDictionary*)iconFamilyID:(int16_t)famID inResourceFile:(RFILE*)rfile;
- (int)iconFamilyIDForType:(OSType)type inBundle:(void*)bndl inResourceFile:(RFILE*)rfile;
- (CGImageRef)iconImageFromFamily:(NSDictionary*)iconFamily;
- (CGImageRef)iconImageWithData:(NSData*)iconData mask:(NSData*)iconMask size:(int)size depth:(int)depth;
- (NSData*)imageDataFromCGImage:(CGImageRef)image;
@end

#define RSHORT(base, offset) ntohs(*((short*)((base)+(offset))))
#define RLONG(base, offset) ntohl(*((long*)((base)+(offset))))
#define RCSTR(base, offset) ((char*)((base)+(offset)))
