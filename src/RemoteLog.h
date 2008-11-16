/*
 *  RemoteLog.h
 *  RemoteLog
 *
 *  Created by Zydeco on 2008-10-12.
 *  Copyright 2008 namedfork.net. All rights reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>

#define kRemoteLogAddress "192.168.0.6"
#define kRemoteLogPort 3344

void RemoteLog(CFStringRef format, ...);