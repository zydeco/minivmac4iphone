/*
 * dskicon - utility to create icons from Macintosh disks
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

#import <Foundation/Foundation.h>
#import <sys/xattr.h>
#import "DSKIconFactory.h"

// usage: dskicon [output_file|-x:attr] disk_image
// will determine an appropiate icon and write it as png to output_file
// returns 0 on success

int main (int argc, char const *argv[])
{
    if (argc < 3) {
        printf("usage: %s output disk_image\n", argv[0]);
        printf("use -x:attr as output to set the icon as an extended attribute\n");
        printf("returns the number of icons created");
        return 1;
    }
    
    int retVal = 0;
    NSAutoreleasePool * pool = [NSAutoreleasePool new];
    NSString * output = [NSString stringWithUTF8String:argv[1]];
    NSString * xattr = nil;
    if ([output hasPrefix:@"-x:"]) xattr = [output substringFromIndex:3];
    
    for(int i=2; i < argc; i++) {
        NSString * diskFile = [[NSString alloc] initWithUTF8String:argv[i]];
        NSLog(@"Creating icon for %@", diskFile);
        NSData *iconData = [[DSKIconFactory sharedInstance] iconForDiskImage:diskFile];
        if (iconData) {
            if (xattr) {
                // write to extended attribute
                setxattr([diskFile fileSystemRepresentation], [xattr UTF8String], [iconData bytes], [iconData length], 0, 0);
            } else [iconData writeToFile:output atomically:NO];
            retVal++;
        } else if (xattr) {
            // remove extended attribute
            removexattr([diskFile fileSystemRepresentation], [xattr UTF8String], 0);
        }
        [diskFile release];
    }
    
    [pool release];
    return retVal;
}