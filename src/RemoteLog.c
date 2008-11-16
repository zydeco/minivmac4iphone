/*
 *  RemoteLog.c
 *  RemoteLog
 *
 *  Created by Zydeco on 2008-10-12.
 *  Copyright 2008 namedfork.net. All rights reserved.
 *
 */

#include <stdarg.h>
#include "RemoteLog.h"

static CFWriteStreamRef rlWriteStream = NULL;

Boolean RemoteLogIsConnected(void);
Boolean RemoteLogConnect(void);

void RemoteLog(CFStringRef format, ...) {
	va_list		args;
	CFStringRef	logText;
	char*	    buffer;
	Boolean		mustFreeBuffer = false;
	
	// check connection
	if (!RemoteLogIsConnected()) RemoteLogConnect();
	
	// create string
	va_start(args, format);
	logText = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, NULL, format, args);
	va_end(args);
	
	// get UTF-8 string
	if ((buffer = (char*)CFStringGetCStringPtr(logText, kCFStringEncodingUTF8)) == NULL) {
		CFIndex bufferSize;
		mustFreeBuffer = true;
		bufferSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(logText), kCFStringEncodingUTF8) + 1;
		buffer = malloc(bufferSize);
		CFStringGetCString(logText, buffer, bufferSize, kCFStringEncodingUTF8);
	}
	
	// write
	CFWriteStreamWrite(rlWriteStream, (const uint8_t*)buffer, strlen(buffer));
	CFWriteStreamWrite(rlWriteStream, (const uint8_t*)"\n", 1);
	
	// free memory
	CFRelease(logText);
	if (mustFreeBuffer) free(buffer);
}

Boolean RemoteLogIsConnected() {
	CFStreamStatus status;
	if (rlWriteStream == nil) return false;
	status = CFWriteStreamGetStatus(rlWriteStream);
	return (status == kCFStreamStatusOpen || 
			status == kCFStreamStatusWriting ||
			status == kCFStreamStatusOpening ||
			status == kCFStreamStatusReading);
}

Boolean RemoteLogConnect() {
	CFStreamCreatePairWithSocketToHost(kCFAllocatorDefault, CFSTR(kRemoteLogAddress), kRemoteLogPort, NULL, &rlWriteStream);
	if (CFWriteStreamOpen(rlWriteStream) == false) {
		CFRelease(rlWriteStream);
		rlWriteStream = NULL;
		return false;
	}
	return true;
}