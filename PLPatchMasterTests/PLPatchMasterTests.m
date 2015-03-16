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

- (void) testRebindSymbol {
#if defined(__i386__) && !defined(TARGET_OS_IPHONE)
    /* ObjC 1.0 runtime */
    NSString *classSymbol = @".objc_class_name__NSAttributedString";
#else
    /* ObjC 2.0 runtime */
    NSString *classSymbol = @"_OBJC_CLASS_$_NSAttributedString";
#endif
    
    /* Fetch the current class pointer */
    void *orig = (__bridge void *) [NSAttributedString class];
    XCTAssertEqual((__bridge void *) [NSAttributedString class], orig);

    /* Rebind */
    [[PLPatchMaster master] rebindSymbol: classSymbol fromImage: @"Foundation" replacementAddress: (uintptr_t) [NSArray class]];
    XCTAssertEqual([NSAttributedString class], [NSArray class]);
    XCTAssertNotEqual((__bridge void *) [NSAttributedString class], orig);
    
    /* Restore the original */
    [[PLPatchMaster master] rebindSymbol: classSymbol fromImage: @"Foundation" replacementAddress: (uintptr_t) orig];
    XCTAssertEqual((__bridge void *) [NSAttributedString class], orig);
}

@end
