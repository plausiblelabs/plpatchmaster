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

#import <Foundation/Foundation.h>
#import <libkern/OSAtomic.h>
#import "SymbolBinder.hpp"

using namespace patchmaster;

/**
 * @internal
 *
 * Table of symbol-based patches; maps the single-level symbol name to the
 * fully qualified two-level SymbolNames and associated patch value.
 */
typedef std::map<std::string, std::vector<std::tuple<SymbolName, uintptr_t>>> PatchTable;

@interface PLPatchMasterImpl : NSObject {
    /** Lock that must be held when mutating or accessing internal state */
    OSSpinLock _lock;
    
    IMP _callbackFunc;
    
    /**
     * Table of symbol-based patches; maps the single-level symbol name to the
     * fully qualified two-level SymbolNames and associated patch value.
     */
    PatchTable _symbolPatches;
    
    /** Maps class -> set -> selector names. Used to keep track of patches that have already been made,
     * and thus do not require a _restoreBlock to be registered */
    NSMutableDictionary *_classPatches;
    
    /** Maps class -> set -> selector names. Used to keep track of patches that have already been made,
     * and thus do not require a _restoreBlock to be registered */
    NSMutableDictionary *_instancePatches;
    
    /** An array of blocks to be executed on dynamic library load; the blocks are responsible
     * for applying any pending patches to the newly loaded library */
    NSMutableArray *_pendingPatches;
    
    /* An array of zero-arg blocks that, when executed, will reverse
     * all previously patched methods. */
    NSMutableArray *_restoreBlocks;
}

- (BOOL) patchClass: (Class) cls selector: (SEL) selector replacementBlock: (id) replacementBlock;
- (BOOL) patchInstancesWithClass: (Class) cls selector: (SEL) selector replacementBlock: (id) replacementBlock;

- (void) patchFutureClassWithName: (NSString *) className selector: (SEL) selector replacementBlock: (id) replacementBlock;
- (void) patchInstancesWithFutureClassName: (NSString *) className selector: (SEL) selector replacementBlock: (id) replacementBlock;

- (void) rebindSymbol: (NSString *) symbol fromImage: (NSString *) library replacementAddress: (uintptr_t) replacementAddress;
- (void) rebindSymbol: (NSString *) symbol replacementAddress: (uintptr_t) replacementAddress;

@end
