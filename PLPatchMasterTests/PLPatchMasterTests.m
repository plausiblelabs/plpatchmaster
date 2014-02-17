//
//  PLPatchMasterTests.m
//  PLPatchMasterTests
//
//  Created by Landon Fuller on 2/17/14.
//
//

#import <XCTest/XCTest.h>
#import "PLPatchMaster.h"

@interface PLPatchMasterTests : XCTestCase

@end

@implementation PLPatchMasterTests

- (NSString *) patchTarget {
    return @"Result";
}

- (void) testExample {
    [PLPatchMaster class];
    
    [PLPatchMasterTests pl_patchInstanceSelector: @selector(patchTarget) withReplacementBlock: ^(PLPatchIMP *patch) {
        NSObject *obj = PLPatchGetSelf(patch);
        XCTAssert(obj == self);
        NSString *originalResult = PLPatchIMPFoward(patch, NSString *(*)(id, SEL));
        return [NSString stringWithFormat: @"[PATCHED]: %@", originalResult];
    }];
    
    XCTAssertEqualObjects(@"[PATCHED]: Result", [self patchTarget]);
}

@end
