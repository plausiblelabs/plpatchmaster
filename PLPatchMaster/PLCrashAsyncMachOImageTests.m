/*
 * Mach-O parsing support, originally written for PLCrashReporter;
 * modified to remove support for out-of-process memory mapping
 * and to work with XCTest.
 *
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2015 Landon Fuller <landon@landonf.org>
 * Copyright (c) 2008-2013 Plausible Labs Cooperative, Inc.
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

#import "PLCrashAsyncMachOImage.h"
#include <XCTest/XCTest.h>

#import <dlfcn.h>
#import <mach-o/dyld.h>
#import <mach-o/getsect.h>
#import <objc/runtime.h>
#import <execinfo.h>

@interface PLCrashAsyncMachOImageTests : XCTestCase {
    /** The image containing our class. */
    plcrash_async_macho_t _image;
}
@end


@implementation PLCrashAsyncMachOImageTests

- (void) setUp {
    /* Fetch our containing image's dyld info */
    Dl_info info;
    const void *classPtr = (__bridge const void *) [self class];
    XCTAssertTrue((dladdr(classPtr, &info) > 0), @"Could not fetch dyld info for %p", [self class]);

    /* Look up the vmaddr and slide for our image */
    uintptr_t text_vmaddr;
    intptr_t vmaddr_slide = 0;
    bool found_image = false;
    for (uint32_t i = 0; i < _dyld_image_count(); i++) {
        if (_dyld_get_image_header(i) == info.dli_fbase) {
            vmaddr_slide = _dyld_get_image_vmaddr_slide(i);
            text_vmaddr = (uintptr_t) (info.dli_fbase - vmaddr_slide);
            found_image = true;
            break;
        }
    }
    XCTAssertTrue(found_image, @"Could not find dyld image record");

    plcrash_nasync_macho_init(&_image, info.dli_fname, (uintptr_t) info.dli_fbase);

    /* Basic test of the initializer */
    XCTAssertTrue(strcmp(_image.name, info.dli_fname) == 0, @"Incorrect name");
    XCTAssertEqual(_image.header_addr, (uintptr_t) info.dli_fbase, @"Incorrect header address");
    XCTAssertEqual(_image.vmaddr_slide, (intptr_t) vmaddr_slide, @"Incorrect vmaddr_slide value");
    
    unsigned long text_size;
    XCTAssertTrue(getsegmentdata(info.dli_fbase, SEG_TEXT, &text_size) != NULL, @"Failed to find segment");
    XCTAssertEqual(_image.text_size, (size_t) text_size, @"Incorrect text segment size computed");
    XCTAssertEqual(_image.text_vmaddr, (uintptr_t) text_vmaddr, @"Incorrect text segment address computed");
}

- (void) tearDown {
    plcrash_nasync_macho_free(&_image);
}

/**
 * Test Mach header getters.
 */
- (void) testMachHeader {
    XCTAssertEqual((const struct mach_header *)&_image.header, plcrash_async_macho_header(&_image), @"Returned incorrect header");

    if (_image.m64) {
        XCTAssertEqual((size_t)sizeof(struct mach_header_64), plcrash_async_macho_header_size(&_image), @"Incorrect header size");
    } else {
        XCTAssertEqual((size_t)sizeof(struct mach_header), plcrash_async_macho_header_size(&_image), @"Incorrect header size");
    }
}

/** Address range testing. */
- (void) testContainsAddress {
    XCTAssertTrue(plcrash_async_macho_contains_address(&_image, _image.header_addr), @"The base address should be contained within the image");
    XCTAssertTrue(_image.header_addr > 0, @"This should always be true ...");
    XCTAssertFalse(plcrash_async_macho_contains_address(&_image, _image.header_addr-1), @"Returned true for an address outside the mapped range");

    XCTAssertFalse(plcrash_async_macho_contains_address(&_image, _image.header_addr+_image.text_size), @"Returned true for an address outside the mapped range");
    XCTAssertTrue(plcrash_async_macho_contains_address(&_image, _image.header_addr+_image.text_size-1), @"The final byte should be within the mapped range");
}

/**
 * Test CPU type/subtype getters.
 */
- (void) testCPUType {
    /* Modify the image to enable byte order handling */
    _image.header.cputype = CPU_TYPE_X86;
    _image.header.cpusubtype = CPU_SUBTYPE_586;

    /* Verify the result */
    XCTAssertEqual(CPU_TYPE_X86, plcrash_async_macho_cpu_type(&_image), @"Incorrect CPU type");
    XCTAssertEqual(CPU_SUBTYPE_586, plcrash_async_macho_cpu_subtype(&_image), @"Incorrect CPU subtype");
}

/**
 * Test iteration of Mach-O load commands.
 */
- (void) testIterateCommand {

    plcrash_async_macho_t image;
    for (uint32_t i = 0; i < _dyld_image_count(); i++) {
        plcrash_nasync_macho_init(&image, _dyld_get_image_name(i), (uintptr_t) _dyld_get_image_header(i));
        struct load_command *cmd = NULL;

        for (uint32_t ncmd = 0; ncmd < image.ncmds; ncmd++) {
            cmd = plcrash_async_macho_next_command(&image, cmd);
            XCTAssertTrue(cmd != NULL, @"Failed to fetch load command %" PRIu32 " of %" PRIu32 "in %s", ncmd, image.ncmds, image.name);

            if (cmd == NULL)
                break;

            XCTAssertNotEqual((uint32_t)0, cmd->cmdsize, @"This test simply ensures that dereferencing the cmd pointer doesn't crash: %d:%d:%s", ncmd, image.ncmds, image.name);
        }

        plcrash_nasync_macho_free(&image);
    }
}

/**
 * Test type-specific iteration of Mach-O load commands.
 */
- (void) testIterateSpecificCommand {
    struct load_command *cmd = 0;
    
    bool found_uuid = false;

    while ((cmd = plcrash_async_macho_next_command_type(&_image, cmd, LC_UUID)) != 0) {
        /* Validate the command type and size */
        XCTAssertEqual(cmd->cmd, (uint32_t)LC_UUID, @"Incorrect load command returned");
        XCTAssertEqual((size_t)cmd->cmdsize, sizeof(struct uuid_command), @"Incorrect load command size returned by iterator");

        XCTAssertFalse(found_uuid, @"Duplicate LC_UUID load commands iterated");
        found_uuid = true;
    }

    XCTAssertTrue(found_uuid, @"Failed to iterate LC_CMD structures");
    
    /* Test the case where there are no matches. LC_SUB_UMBRELLA should never be used in a unit tests binary. */
    cmd = plcrash_async_macho_next_command_type(&_image, NULL, LC_SUB_UMBRELLA);
    XCTAssertTrue(cmd == NULL, @"Should not have found the requested load command");
}

/**
 * Test type-specific iteration of Mach-O load commands when a NULL size argument is provided.
 */
- (void) testIterateSpecificCommandNULLSize {
    struct load_command *cmd = NULL;
    
    /* If the following doesn't crash dereferencing the NULL cmdsize argument, success! */
    bool found_uuid = false;
    while ((cmd = plcrash_async_macho_next_command_type(&_image, cmd, LC_UUID)) != 0) {
        XCTAssertFalse(found_uuid, @"Duplicate LC_UUID load commands iterated");
        found_uuid = true;
    }
    
    XCTAssertTrue(found_uuid, @"Failed to iterate LC_CMD structures");
}

/**
 * Test simple short-cut for finding a single load_command.
 */
- (void) testFindCommand {
    struct load_command *cmd = plcrash_async_macho_find_command(&_image, LC_UUID);
    XCTAssertTrue(cmd != NULL, @"Failed to find command");
    XCTAssertEqual(cmd->cmd, (uint32_t)LC_UUID, @"Incorrect load command returned");
    XCTAssertEqual(cmd->cmdsize, (uint32_t)sizeof(struct uuid_command), @"Incorrect load command size returned");
    
    /* Test the case where there are no matches. LC_SUB_UMBRELLA should never be used in a unit tests binary. */
    cmd = plcrash_async_macho_find_command(&_image, LC_SUB_UMBRELLA);
    XCTAssertTrue(cmd == NULL, @"Should not have found the requested load command");
}

/**
 * Test memory mapping of a Mach-O segment
 */
- (void) testMapSegment {
    pl_async_macho_mapped_segment_t seg;

    /* Try to map the segment */
    XCTAssertEqual(PLCRASH_ESUCCESS, plcrash_async_macho_map_segment(&_image, "__TEXT", &seg), @"Failed to map segment");
    
    /* Fetch the segment directly for comparison */
    unsigned long segsize = 0;
    uint8_t *data = getsegmentdata((void *)_image.header_addr, "__TEXT", &segsize);
    XCTAssertTrue(data != NULL, @"Could not fetch segment data");

    /* Compare the address and length.  */
    XCTAssertEqual((uintptr_t)data, (uintptr_t) seg.mobj.address, @"Addresses do not match");
    XCTAssertEqual((size_t)segsize, seg.mobj.length, @"Sizes do not match");
    
    /* Fetch the segment command for further comparison */
    struct load_command *cmd = plcrash_async_macho_find_segment_cmd(&_image, "__TEXT");
    XCTAssertTrue(data != NULL, @"Could not fetch segment command");
    if (cmd->cmd == LC_SEGMENT) {
        struct segment_command *segcmd = (struct segment_command *) cmd;
        XCTAssertEqual(seg.fileoff, (uint64_t) segcmd->fileoff, @"File offset does not match");
        XCTAssertEqual(seg.filesize, (uint64_t) segcmd->filesize, @"File size does not match");

    } else if (cmd->cmd == LC_SEGMENT_64) {
        struct segment_command_64 *segcmd = (struct segment_command_64 *) cmd;
        XCTAssertEqual(seg.fileoff, segcmd->fileoff, @"File offset does not match");
        XCTAssertEqual(seg.filesize, segcmd->filesize, @"File size does not match");
    } else {
        XCTFail(@"Unsupported command type!");
    }

    /* Clean up */
    plcrash_async_macho_mapped_segment_free(&seg);

    /* Test handling of a missing segment */
    XCTAssertEqual(PLCRASH_ENOTFOUND, plcrash_async_macho_map_segment(&_image, "__NO_SUCH_SEG", &seg), @"Should have failed to map the segment");
}

/**
 * Test memory mapping of a Mach-O section
 */
- (void) testMapSection {
    plcrash_async_mobject_t mobj;
    
    /* Try to map the section */
    XCTAssertEqual(PLCRASH_ESUCCESS, plcrash_async_macho_map_section(&_image, "__DATA", "__const", &mobj), @"Failed to map section");
    
    /* Fetch the section directly for comparison */
    unsigned long sectsize = 0;
    uint8_t *data = getsectiondata((void *)_image.header_addr, "__DATA", "__const", &sectsize);
    XCTAssertTrue(data != NULL, @"Could not fetch section data");

    /* Compare the address and length. We have to apply the slide to determine the original source address. */
    XCTAssertEqual((uintptr_t)data, (uintptr_t) mobj.address, @"Addresses do not match");
    XCTAssertEqual((size_t)sectsize, mobj.length, @"Sizes do not match");

    /* Test handling of a missing section */
    XCTAssertEqual(PLCRASH_ENOTFOUND, plcrash_async_macho_map_section(&_image, "__DATA", "__NO_SUCH_SECT", &mobj), @"Should have failed to map the section");
}


/**
 * Test memory mapping of a missing Mach-O segment
 */
- (void) testMapMissingSegment {
    pl_async_macho_mapped_segment_t seg;
    XCTAssertEqual(PLCRASH_ENOTFOUND, plcrash_async_macho_map_segment(&_image, "__NO_SUCH_SEG", &seg), @"Should have failed to map the segment");
}

/* testFindSymbol callback handling */

struct testFindSymbol_cb_ctx {
    uintptr_t addr;
    char *name;
};

static void testFindSymbol_cb (uintptr_t address, const char *name, void *ctx) {
    struct testFindSymbol_cb_ctx *cb_ctx = ctx;
    cb_ctx->addr = address;
    cb_ctx->name = strdup(name);
}

/**
 * Test basic initialization of the symbol table reader.
 */
- (void) testInitSymtabReader {
    plcrash_async_macho_symtab_reader_t reader;
    plcrash_error_t ret = plcrash_async_macho_symtab_reader_init(&reader, &_image);
    XCTAssertEqual(ret, PLCRASH_ESUCCESS, @"Failed to initializer reader");
    
    XCTAssertTrue(reader.symtab != NULL, @"Failed to map symtab");
    XCTAssertTrue(reader.symtab_global != NULL, @"Failed to map global symtab");
    XCTAssertTrue(reader.symtab_local != NULL, @"Failed to map global symtab");
    XCTAssertTrue(reader.string_table != NULL, @"Failed to map string table");
    
    /* Try iterating the tables. If we don't crash, we're doing well. */
    plcrash_async_macho_symtab_entry_t entry;
    for (uint32_t i = 0; i <reader.nsyms; i++) {
        entry = plcrash_async_macho_symtab_reader_read(&reader, reader.symtab, i);
        
        /* If the symbol is not within a section, or a debugging symbol, skip the remaining tests */
        if ((entry.n_type & N_TYPE) != N_SECT || ((entry.n_type & N_STAB) != 0))
            continue;

        const char *sym = plcrash_async_macho_symtab_reader_symbol_name(&reader, entry.n_strx);
        XCTAssertTrue(sym != NULL, @"Symbol name read failed");
    }

    for (uint32_t i = 0; i <reader.nsyms_global; i++)
        entry = plcrash_async_macho_symtab_reader_read(&reader, reader.symtab_global, i);
    
    for (uint32_t i = 0; i <reader.nsyms_local; i++)
        entry = plcrash_async_macho_symtab_reader_read(&reader, reader.symtab_local, i);

    plcrash_async_macho_symtab_reader_free(&reader);
}

/**
 * Test indirect table reading.
 */
- (void) testReadIndirect {
    plcrash_async_macho_symtab_reader_t reader;
    plcrash_error_t ret = plcrash_async_macho_symtab_reader_init(&reader, &_image);
    XCTAssertEqual(ret, PLCRASH_ESUCCESS, @"Failed to initializer reader");

    /* We should be able to find an indirect symbol reference to _dladdr :-) */
    const char *sym;
    plcrash_async_macho_symtab_entry_t entry;
    for (uint32_t i = 0; i < reader.indirect_table_count; i++) {
        uint32_t sym_idx = plcrash_async_macho_symtab_reader_indirect(&reader, i);
        if (sym_idx >= reader.nsyms)
            continue;
        
        entry = plcrash_async_macho_symtab_reader_read(&reader, reader.symtab, sym_idx);

        /* Verify the name */
        sym = plcrash_async_macho_symtab_reader_symbol_name(&reader, entry.n_strx);
        if (sym != NULL && strcmp(sym, "_dladdr") == 0)
            break;
    }

    /* Verify the name */
    XCTAssertTrue(sym != NULL, @"Symbol name read failed");
    if (sym != NULL)
        XCTAssertTrue(strcmp(sym, "_dladdr") == 0, @"Returned incorrect symbol name: %s != %s", sym, "_dladdr");
    
    plcrash_async_macho_symtab_reader_free(&reader);
}

/**
 * Test symbol name reading.
 */
- (void) testReadSymbolName {
    /* Fetch the our IMP address and symbolicate it using dladdr(). */
    IMP localIMP = class_getMethodImplementation([self class], _cmd);
    Dl_info dli;
    XCTAssertTrue(dladdr((void *)localIMP, &dli) != 0, @"Failed to look up symbol");
    XCTAssertTrue(dli.dli_sname != NULL, @"Symbol name was stripped!");
    
    /* Now walk the Mach-O table ourselves */
    plcrash_async_macho_symtab_reader_t reader;
    plcrash_error_t ret = plcrash_async_macho_symtab_reader_init(&reader, &_image);
    XCTAssertEqual(ret, PLCRASH_ESUCCESS, @"Failed to initializer reader");

    /* Find the symbol entry and extract the name name */
    const char *sym = NULL;
    plcrash_async_macho_symtab_entry_t entry;
    for (uint32_t i = 0; i < reader.nsyms; i++) {
        entry = plcrash_async_macho_symtab_reader_read(&reader, reader.symtab, i);
        /* Skip non-matching symbols */
        if (entry.normalized_value != (uintptr_t) dli.dli_saddr - _image.vmaddr_slide)
            continue;
        
        /* If the symbol is not within a section, or a debugging symbol, skip the remaining tests */
        if ((entry.n_type & N_TYPE) != N_SECT || ((entry.n_type & N_STAB) != 0))
            continue;
        
        /* Verify the name */
        sym = plcrash_async_macho_symtab_reader_symbol_name(&reader, entry.n_strx);
    }
    
    XCTAssertTrue(sym != NULL, @"Symbol name read failed");
    if (sym != NULL)
        XCTAssertTrue(strcmp(sym, dli.dli_sname) == 0, @"Returned incorrect symbol name: %s != %s", sym, dli.dli_sname);

    plcrash_async_macho_symtab_reader_free(&reader);
}

/**
 * Test symbol lookup.
 */
- (void) testFindSymbol {
    /* Fetch our current PC, to be used for symbol lookup */
    void *callstack[1];
    int frames = backtrace(callstack, 1);
    XCTAssertEqual(1, frames, @"Could not fetch our PC");

    /* Perform our symbol lookup */
    struct testFindSymbol_cb_ctx ctx;
    plcrash_error_t res = plcrash_async_macho_find_symbol_by_pc(&_image, (uintptr_t) callstack[0], testFindSymbol_cb, &ctx);
    XCTAssertEqual(res, PLCRASH_ESUCCESS, @"Failed to locate symbol");
    
    /* The following tests will crash if the above did not succeed */
    if (res != PLCRASH_ESUCCESS)
        return;
    
    /* Fetch the our IMP address and symbolicate it using dladdr(). */
    IMP localIMP = class_getMethodImplementation([self class], _cmd);
    Dl_info dli;
    XCTAssertTrue(dladdr((void *)localIMP, &dli) != 0, @"Failed to look up symbol");

    /* Compare the results */
    XCTAssertTrue(strcmp(dli.dli_sname, ctx.name) == 0, @"Returned incorrect symbol name");
    XCTAssertEqual(dli.dli_saddr, (void *) ctx.addr, @"Returned incorrect symbol address with slide %" PRId64, (int64_t) _image.vmaddr_slide);
}

/**
 * Test lookup of symbols by name.
 */
- (void) testFindSymbolByName {
    /* Fetch our current symbol name, to be used for symbol lookup */
    IMP localIMP = class_getMethodImplementation([self class], _cmd);
    Dl_info dli;
    XCTAssertTrue(dladdr((void *)localIMP, &dli) != 0, @"Failed to look up symbol");

    /* Perform our symbol lookup */
    uintptr_t pc;
    plcrash_error_t res = plcrash_async_macho_find_symbol_by_name(&_image, dli.dli_sname, &pc);
    XCTAssertEqual(res, PLCRASH_ESUCCESS, @"Failed to locate symbol %s", dli.dli_sname);

    /* Compare the results */
    XCTAssertEqual((uintptr_t) localIMP, pc, @"Returned incorrect symbol address");
}

@end
