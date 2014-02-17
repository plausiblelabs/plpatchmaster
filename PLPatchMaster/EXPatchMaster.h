/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2013 Plausible Labs Cooperative, Inc.
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

#import <Foundation/Foundation.h>

/**
 * IMP patch state, as passed to a replacement block.
 */
typedef struct EXPatchIMP {
    /** The original message target. */
    void *self;
    
    /** The original IMP (eg, the IMP prior to patching) */
    IMP origIMP;

    /** The original SEL. */
    SEL selector;
} EXPatchIMP;

/**
 * Forward a message received by a EXPatchMaster patch block.
 *
 * @param patch The EXPatchIMP patch argument.
 * @param func_type The function type to which the IMP should be cast.
 * @param ... All method arguments (Do not include self or _cmd).
 */
#define EXPatchIMPFoward(patch, func_type, ...) ((func_type)patch->origIMP)((__bridge id) patch->self, patch->selector, ##__VA_ARGS__)

@interface NSObject (EXPatchMaster)

+ (BOOL) ex_patchSelector: (SEL) selector withReplacementBlock: (id) replacementBlock;
+ (BOOL) ex_patchInstanceSelector: (SEL) selector withReplacementBlock: (id) replacementBlock;

+ (void) ex_patchFutureSelector: (SEL) selector withReplacementBlock: (id) replacementBlock;
+ (void) ex_patchFutureInstanceSelector: (SEL) selector withReplacementBlock: (id) replacementBlock;

@end

@interface EXPatchMaster : NSObject

+ (instancetype) master;

- (BOOL) patchClass: (Class) cls selector: (SEL) selector replacementBlock: (id) replacementBlock;
- (BOOL) patchInstancesWithClass: (Class) cls selector: (SEL) selector replacementBlock: (id) replacementBlock;

- (void) patchFutureClassWithName: (NSString *) className selector: (SEL) selector replacementBlock: (id) replacementBlock;
- (void) patchInstancesWithFutureClassName: (NSString *) className selector: (SEL) selector replacementBlock: (id) replacementBlock;

@end
