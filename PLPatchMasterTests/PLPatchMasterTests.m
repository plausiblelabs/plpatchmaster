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

- (void) testBasic {
    [PLPatchMasterTests pl_patchInstanceSelector: @selector(patchTargetWithArgument:) withReplacementBlock: ^(PLPatchIMP *patch, NSString *expected) {
        NSObject *obj = PLPatchGetSelf(patch);
        XCTAssertTrue(obj == self, @"Incorrect 'self'");
        NSString *originalResult = PLPatchIMPFoward(patch, NSString *(*)(id, SEL, NSString *), expected);
        return [NSString stringWithFormat: @"[PATCHED]: %@", originalResult];
    }];
    
    XCTAssertEqualObjects(@"[PATCHED]: Result", [self patchTargetWithArgument: @"Result"], @"Incorrect value returned");
}

struct stret_return {
    char value[30];
};

- (struct stret_return) stretPatchTargetWithArgument: (NSString *) expected {
    struct stret_return retval;
    const char *cstr = [expected UTF8String];
    
    assert(strlen(cstr) < sizeof(retval.value));
    strlcpy(retval.value, cstr, sizeof(retval.value));

    return retval;
}

- (void) testStret {
    [PLPatchMasterTests pl_patchInstanceSelector: @selector(stretPatchTargetWithArgument:) withReplacementBlock: ^(PLPatchIMP *patch, NSString *expected) {
        NSObject *obj = PLPatchGetSelf(patch);
        XCTAssertTrue(obj == self, @"Incorrect 'self'");

        struct stret_return retval = PLPatchIMPFoward(patch, struct stret_return (*)(id, SEL, NSString *), expected);
        retval.value[0] = 'j';
        return retval;
    }];

    struct stret_return ret;
    ret.value[0] = 'f';
    ret.value[1] = '\0';

    ret = [self stretPatchTargetWithArgument: @"hello"];
    XCTAssertTrue(strcmp(ret.value, "jello") == 0, @"Incorrect value returned: '%s'", ret.value);
}

static CFIndex patched_CFGetRetainCount (CFTypeRef ref) {
    return 0xABBA;
}

- (void) testRebindSymbol {
    /* Fetch the current function pointer */
    CFIndex (*orig)(CFTypeRef) = &CFGetRetainCount;
    
    /* Rebind */
    [[PLPatchMaster master] rebindSymbol: @"_CFGetRetainCount" fromImage: kPLPatchImageCoreFoundation replacementAddress: (uintptr_t) patched_CFGetRetainCount];
    XCTAssertEqual(0xABBA, CFGetRetainCount((__bridge CFTypeRef) [NSArray array]));
    
    /* Restore the original */
    [[PLPatchMaster master] rebindSymbol: @"_CFGetRetainCount" fromImage: kPLPatchImageCoreFoundation replacementAddress: (uintptr_t) orig];
    XCTAssertNotEqual(0xABBA, CFGetRetainCount((__bridge CFTypeRef) [NSArray array]));
}

#if !defined(__i386__) || (defined(__i386__) && TARGET_OS_IPHONE)
- (void) testRebindClassSymbol {
    /* This test only works on the ObjC 2.0 runtime; the ObjC 1.0 runtime performs
     * class binding itself, rather than via dyld */
    NSString *classSymbol = @"_OBJC_CLASS_$_NSAttributedString";
    
    /* Fetch the current class pointer */
    void *orig = (__bridge void *) [NSAttributedString class];
    XCTAssertEqualObjects([NSAttributedString class], (__bridge id) orig);

    /* Rebind */
    [[PLPatchMaster master] rebindSymbol: classSymbol fromImage: @"Foundation" replacementAddress: (uintptr_t) [NSArray class]];
    XCTAssertEqualObjects([NSAttributedString class], [NSArray class]);
    XCTAssertNotEqualObjects([NSAttributedString class], (__bridge id) orig);
    
    /* Restore the original */
    [[PLPatchMaster master] rebindSymbol: classSymbol fromImage: @"Foundation" replacementAddress: (uintptr_t) orig];
    XCTAssertEqualObjects([NSAttributedString class], (__bridge id) orig);
}
#endif

@end

