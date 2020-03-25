//
//  SystemMonitor.h
//  bVNC
//
//  Created by iordan iordanov on 2020-03-24.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//
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
