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

- (NSString *) patchTargetWithArgument: (NSString *) expected {
    return expected;
}

- (void) testExample {
    [PLPatchMaster class];
    
    [PLPatchMasterTests pl_patchInstanceSelector: @selector(patchTargetWithArgument:) withReplacementBlock: ^(PLPatchIMP *patch, NSString *expected) {
        NSObject *obj = PLPatchGetSelf(patch);
        XCTAssert(obj == self);
        NSString *originalResult = PLPatchIMPFoward(patch, NSString *(*)(id, SEL, NSString *), expected);
        return [NSString stringWithFormat: @"[PATCHED]: %@", originalResult];
    }];
    
    XCTAssertEqualObjects(@"[PATCHED]: Result", [self patchTargetWithArgument: @"Result"]);
}

@end
