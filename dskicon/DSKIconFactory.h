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

#import <Foundation/Foundation.h>

@interface DSKIconFactory : NSObject
{
}

+ (DSKIconFactory*)sharedInstance;
- (NSData*)iconForDiskImage:(NSString*)path;
- (NSData*)iconForMFSDiskImage:(NSString*)path withOffset:(long)offset;
- (NSData*)iconForHFSDiskImage:(NSString*)path withOffset:(long)offset;
@end

