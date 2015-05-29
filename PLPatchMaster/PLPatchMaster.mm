/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2013-2015 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#import "PLPatchMaster.h"
#import "PLPatchMasterImpl.hpp"

/** Foundation.framework's library install name. */
NSString *kPLPatchImageFoundation = @"/System/Library/Frameworks/Foundation.framework/Versions/C/Foundation";

/** CoreFoundation.framework's library install name. */
NSString *kPLPatchImageCoreFoundation = @"/System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation";

/** libSystem.dylib's library install name. */
NSString *kPLPatchImageLibSystem = @"/usr/lib/libSystem.B.dylib";

/**
 * Manages application (and removal) of runtime patches. This class is thread-safe, and may be accessed from any thread.
 */
@implementation PLPatchMaster

/**
 * Return the default patch master.
 */
+ (instancetype) master {
    static PLPatchMaster *m = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        m = [[PLPatchMaster alloc] init];;
    });
    
    return m;
}

- (instancetype) init {
    if ((self = [super init]) == nil)
        return nil;
    
    /* Default state */
    _impl = [[PLPatchMasterImpl alloc] init];

    return self;
}

- (void) dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver: self];
    [_impl release];
    [super dealloc];
}


/**
 * Patch the class method @a selector of @a className, where @a className may not yet have been loaded,
 * or @a selector may not yet have been registered by a category.
 *
 * This may be used to register patches that will be automatically applied when the bundle or framework
 * to which they apply is loaded.
 *
 * @param className The name of the class to patch. The class may not yet have been loaded.
 * @param selector The selector to patch.
 * @param replacementBlock The new implementation for @a selector. The first parameter must be a pointer to PLPatchIMP; the
 * remainder of the parameters must match the original method.
 */
- (void) patchFutureClassWithName: (NSString *) className selector: (SEL) selector replacementBlock: (id) replacementBlock {
    [_impl patchFutureClassWithName: className selector: selector replacementBlock: replacementBlock];
}

/**
 * Patch the instance method @a selector of @a className, where @a className may not yet have been loaded,
 * or @a selector may not yet have been registered by a category.
 *
 * This may be used to register patches that will be automatically applied when the bundle or framework
 * to which they apply is loaded.
 *
 * @param className The name of the class to patch. The class may not yet have been loaded.
 * @param selector The selector to patch.
 * @param replacementBlock The new implementation for @a selector. The first parameter must be a pointer to PLPatchIMP; the
 * remainder of the parameters must match the original method.
 */
- (void) patchInstancesWithFutureClassName: (NSString *) className selector: (SEL) selector replacementBlock: (id) replacementBlock {
    [_impl patchInstancesWithFutureClassName: className selector: selector replacementBlock: replacementBlock];
}

/**
 * Patch the class method @a selector of @a cls.
 *
 * @param cls The class to patch.
 * @param selector The selector to patch.
 * @param replacementBlock The new implementation for @a selector. The first parameter must be a pointer to PLPatchIMP; the
 * remainder of the parameters must match the original method.
 *
 * @return Returns YES on success, or NO if @a selector is not a defined @a cls method.
 */
- (BOOL) patchClass: (Class) cls selector: (SEL) selector replacementBlock: (id) replacementBlock {
    return [_impl patchClass: cls selector: selector replacementBlock: replacementBlock];
}

/**
 * Patch the instance method @a selector of @a cls.
 *
 * @param cls The class to patch.
 * @param selector The selector to patch.
 * @param replacementBlock The new implementation for @a selector. The first parameter must be a pointer to PLPatchIMP; the
 * remainder of the parameters must match the original method.
 *
 * @return Returns YES on success, or NO if @a selector is not a defined @a cls instance method.
 */
- (BOOL) patchInstancesWithClass: (Class) cls selector: (SEL) selector replacementBlock: (id) replacementBlock {
    return [_impl patchInstancesWithClass: cls selector: selector replacementBlock: replacementBlock];
}

/**
 * Perform dyld-compatible symbol rebinding of all references to @a symbol defined by @a library across all current
 * and future loaded images.
 *
 * @param symbol The name of the symbol to patch.
 * @param library The absolute or relative path (e.g. 'Foundation') to the library responsible for exporting the original symbol.
 * @param replacementAddress The new address to which
 */
- (void) rebindSymbol: (NSString *) symbol fromImage: (NSString *) library replacementAddress: (uintptr_t) replacementAddress {
    [_impl rebindSymbol: symbol fromImage: library replacementAddress: replacementAddress];
}

/**
 * Perform dyld-compatible symbol rebinding of all references to @a symbol defined by *any* library across all current
 * and future loaded images.
 *
 * This is essentially equivalent to single-level namespace symbol binding.
 *
 * @param symbol The name of the symbol to patch.
 * @param replacementAddress The new address to which
 */
- (void) rebindSymbol: (NSString *) symbol replacementAddress: (uintptr_t) replacementAddress {
    [_impl rebindSymbol: symbol replacementAddress: replacementAddress];
}

@end
