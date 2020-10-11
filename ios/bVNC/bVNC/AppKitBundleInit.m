//
//  AppKitBundleInit.m
//  bVNC
//
//  Created by iordan iordanov on 2020-10-10.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "AppKitBundleInit.h"

@implementation AppKitBundleInit : NSObject

NSString *pluginPath;

-(id)init {
    pluginPath = [[[NSBundle mainBundle] builtInPlugInsPath] stringByAppendingPathComponent:@"AppKitBundle.bundle"];
    [[NSBundle bundleWithPath:pluginPath] load];
    return self;
}

@end
