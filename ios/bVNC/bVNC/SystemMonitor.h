/**
 * Copyright (C) 2021- Morpheusly Inc. All rights reserved.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

// From https://stackoverflow.com/questions/43866416/get-detailed-ios-cpu-usage-with-different-states

#ifndef SystemMonitor_h
#define SystemMonitor_h
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

typedef struct {
    unsigned int system;
    unsigned int user;
    unsigned int nice;
    unsigned int idle;
} CPUUsage;

@interface SystemMonitor: NSObject
+ (CPUUsage)cpuUsage;
+ (CGFloat)appCpuUsage;
@end
#endif /* SystemMonitor_h */
