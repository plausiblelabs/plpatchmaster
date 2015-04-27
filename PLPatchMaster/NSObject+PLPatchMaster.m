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

#import "NSObject+PLPatchMaster.h"
#import "PLPatchMaster.h"

/**
 * Runtime method patching support for NSObject. These are implemented via PLPatchMaster.
 */
@implementation NSObject (PLPatchMaster)

/**
 * Patch the receiver's @a selector class method. The previously registered IMP may be fetched via PLPatchMaster::originalIMP:.
 *
 * @param selector The selector to patch.
 * @param replacementBlock The new implementation for @a selector. The first parameter must be a pointer to PLPatchIMP; the
 * remainder of the parameters must match the original method.
 *
 * @return Returns YES on success, or NO if @a selector is not a defined @a cls method.
 */
+ (BOOL) pl_patchSelector: (SEL) selector withReplacementBlock: (id) replacementBlock {
    return [[PLPatchMaster master] patchClass: [self class] selector: selector replacementBlock: replacementBlock];
}

/**
 * Patch the receiver's @a selector instance method. The previously registered IMP may be fetched via PLPatchMaster::originalIMP:.
 *
 * @param selector The selector to patch.
 * @param replacementBlock The new implementation for @a selector. The first parameter must be a pointer to PLPatchIMP; the
 * remainder of the parameters must match the original method.
 *
 * @return Returns YES on success, or NO if @a selector is not a defined @a cls instance method.
 */
+ (BOOL) pl_patchInstanceSelector: (SEL) selector withReplacementBlock: (id) replacementBlock {
    return [[PLPatchMaster master] patchInstancesWithClass: [self class] selector: selector replacementBlock: replacementBlock];
}

/**
 * Patch the receiver's @a selector class method, once (and if) @a selector is registered by a loaded Mach-O image. The previously
 * registered IMP may be fetched via PLPatchMaster::originalIMP:.
 *
 * @param selector The selector to patch.
 * @param replacementBlock The new implementation for @a selector. The first parameter must be a pointer to PLPatchIMP; the
 * remainder of the parameters must match the original method.
 *
 * @return Returns YES on success, or NO if @a selector is not a defined @a cls method.
 */
+ (void) pl_patchFutureSelector: (SEL) selector withReplacementBlock: (id) replacementBlock {
    return [[PLPatchMaster master] patchFutureClassWithName: NSStringFromClass([self class]) selector: selector replacementBlock: replacementBlock];
    
}

/**
 * Patch the receiver's @a selector instance method, once (and if) @a selector is registered by a loaded Mach-O image.
 * The previously registered IMP may be fetched via PLPatchMaster::originalIMP:.
 *
 * @param selector The selector to patch.
 * @param replacementBlock The new implementation for @a selector. The first parameter must be a pointer to PLPatchIMP; the
 * remainder of the parameters must match the original method.
 *
 * @return Returns YES on success, or NO if @a selector is not a defined @a cls instance method.
 */
+ (void) pl_patchFutureInstanceSelector: (SEL) selector withReplacementBlock: (id) replacementBlock {
    return [[PLPatchMaster master] patchInstancesWithFutureClassName: NSStringFromClass([self class]) selector: selector replacementBlock: replacementBlock];
}

@end

