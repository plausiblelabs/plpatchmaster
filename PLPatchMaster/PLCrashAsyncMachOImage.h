/*
 * Mach-O parsing support, originally written for PLCrashReporter;
 * modified to remove support for out-of-process memory mapping.
 *
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2015 Landon Fuller <landon@landonf.org>
 * Copyright (c) 2011-2013 Plausible Labs Cooperative, Inc.
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

#ifndef PLCRASH_ASYNC_MACHO_IMAGE_H
#define PLCRASH_ASYNC_MACHO_IMAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <mach/mach.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <stdbool.h>

/**
 * @ingroup plcrash_async
 * Error return codes.
 */
typedef enum  {
    /** Success */
    PLCRASH_ESUCCESS = 0,
    
    /** Unknown error (if found, is a bug) */
    PLCRASH_EUNKNOWN,
    
    /** The output file can not be opened or written to */
    PLCRASH_OUTPUT_ERR,
    
    /** No memory available (allocation failed) */
    PLCRASH_ENOMEM,
    
    /** Unsupported operation */
    PLCRASH_ENOTSUP,
    
    /** Invalid argument */
    PLCRASH_EINVAL,
    
    /** Internal error */
    PLCRASH_EINTERNAL,
    
    /** Access to the specified resource is denied. */
    PLCRASH_EACCESS,
    
    /** The requested resource could not be found. */
    PLCRASH_ENOTFOUND,
    
    /** The input data is in an unknown or invalid format. */
    PLCRASH_EINVALID_DATA,
} plcrash_error_t;
    
/**
 * @ingroup plcrash_async
 * @internal
 *
 * An async-accessible memory mapped object.
 */
typedef struct plcrash_async_mobject {
    /** The in-memory address at which the target address has been mapped. This address is offset
     * from the actual starting address, to account for the rounding of mappings to whole pages. */
    uintptr_t address;
    
    /** The total requested length of the mapping. This value is the literal requested length, and is not rounded up to
     * the actual page size. */
    size_t length;
} plcrash_async_mobject_t;

/**
 * @internal
 * @ingroup plcrash_async_image
 * @{
 */

/**
 * @internal
 *
 * A Mach-O image instance.
 */
typedef struct plcrash_async_macho {
    /** The binary image's header address. */
    uintptr_t header_addr;
    
    /** The binary's dyld-reported reported vmaddr slide. */
    intptr_t vmaddr_slide;

    /** The binary image's name/path. */
    char *name;

    /** The Mach-O header. For our purposes, the 32-bit and 64-bit headers are identical. Note that the header
     * values may require byte-swapping for the local process' use. */
    struct mach_header header;
    
    /** Total size, in bytes, of the in-memory Mach-O header. This may differ from the header field above,
     * as the above field does not include the full mach_header_64 extensions to the mach_header. */
    size_t header_size;

    /** Number of load commands */
    uint32_t ncmds;

    /** Mapped Mach-O load commands */
    plcrash_async_mobject_t load_cmds;

    /** The Mach-O image's __TEXT segment, as defined by the LC_SEGMENT/LC_SEGMENT_64 load command. */
    uintptr_t text_vmaddr;

    /** Total size, in bytes, of the Mach-O image's __TEXT segment, as defined by the LC_SEGMENT/LC_SEGMENT_64 load command. */
    size_t text_size;

    /** If true, the image is 64-bit Mach-O. If false, it is a 32-bit Mach-O image. */
    bool m64;
} plcrash_async_macho_t;

/**
 * @internal
 *
 * A mapped Mach-O segment.
 */
typedef struct plcrash_async_macho_mapped_segment_t {
    /** The segment's mapped memory object */
    plcrash_async_mobject_t mobj;

    /* File offset of this segment. */
    uint64_t	fileoff;
    
    /* File size of the segment. */
	uint64_t	filesize;
} pl_async_macho_mapped_segment_t;

/**
 * @internal
 *
 * A 32-bit/64-bit neutral symbol table entry. The values will be returned in host byte order.
 */
typedef struct plcrash_async_macho_symtab_entry {
    /* Index into the string table */
    uint32_t n_strx;

    /** Symbol type. */
    uint8_t n_type;

    /** Section number. */
    uint8_t n_sect;

    /** Description (see <mach-o/stab.h>). */
    uint16_t n_desc;

    /** Symbol value */
    uintptr_t n_value;

    /**
     * The normalized symbol address. This will include any required bit flags -- such as the ARM thumb high-order
     * bit -- which are not included in the symbol table by default.
     */
    uintptr_t normalized_value;
} plcrash_async_macho_symtab_entry_t;

/**
 * @internal
 *
 * A Mach-O symtab reader. Provides support for iterating the contents of a Mach-O symbol table.
 */
typedef struct plcrash_async_macho_symtab_reader {
    /** The image from which the symbol table has been mapped. */
    plcrash_async_macho_t *image;

    /** The mapped LINKEDIT segment. */
    pl_async_macho_mapped_segment_t linkedit;

    /** Pointer to the symtab table within the mapped linkedit segment. The validity of this pointer (and the length of
     * data available) is gauranteed. */
    void *symtab;
    
    /** Total number of elements in the symtab. */
    uint32_t nsyms;
    
    /** Pointer to the global symbol table, if available. May be NULL. The validity of this pointer (and the length of
     * data available) is gauranteed. If non-NULL, symtab_local must also be non-NULL. */
    void *symtab_global;

    /** Total number of elements in the global symtab. */
    uint32_t nsyms_global;

    /** Pointer to the local symbol table, if available. May be NULL. The validity of this pointer (and the length of
     * data available) is gauranteed. If non-NULL, symtab_global must also be non-NULL. */
    void *symtab_local;

    /** Total number of elements in the local symtab. */
    uint32_t nsyms_local;
    
    /** Pointer to the indirect table, if any. May be NULL. */
    uint32_t *indirect_table;

    /** Total number of elements in the symbol table. */
    uint32_t indirect_table_count;

    /** The mapped string table. The validity of this pointer (and the length of
     * data available) is gauranteed. */
    char *string_table;

    /** The string table's size, in bytes. */
    size_t string_table_size;
} plcrash_async_macho_symtab_reader_t;

/**
 * Prototype of a callback function used to execute user code with async-safely fetched symbol.
 *
 * @param address The symbol address.
 * @param name The symbol name. The callback is responsible for copying this value, as its backing storage is not gauranteed to exist
 * after the callback returns.
 * @param ctx The API client's supplied context value.
 */
typedef void (*pl_async_macho_found_symbol_cb)(uintptr_t address, const char *name, void *ctx);

plcrash_error_t plcrash_nasync_macho_init (plcrash_async_macho_t *image, const char *name, uintptr_t header);

const struct mach_header *plcrash_async_macho_header (plcrash_async_macho_t *image);
size_t plcrash_async_macho_header_size (plcrash_async_macho_t *image);
    
bool plcrash_async_macho_contains_address (plcrash_async_macho_t *image, uintptr_t address);

cpu_type_t plcrash_async_macho_cpu_type (plcrash_async_macho_t *image);
cpu_subtype_t plcrash_async_macho_cpu_subtype (plcrash_async_macho_t *image);

void *plcrash_async_macho_next_command (plcrash_async_macho_t *image, void *previous);
void *plcrash_async_macho_next_command_type (plcrash_async_macho_t *image, void *previous, uint32_t expectedCommand);
void *plcrash_async_macho_find_command (plcrash_async_macho_t *image, uint32_t cmd);
void *plcrash_async_macho_find_segment_cmd (plcrash_async_macho_t *image, const char *segname);

plcrash_error_t plcrash_async_macho_map_segment (plcrash_async_macho_t *image, const char *segname, pl_async_macho_mapped_segment_t *seg);
plcrash_error_t plcrash_async_macho_map_section (plcrash_async_macho_t *image, const char *segname, const char *sectname, plcrash_async_mobject_t *mobj);

plcrash_error_t plcrash_async_macho_find_symbol_by_pc (plcrash_async_macho_t *image, uintptr_t pc, pl_async_macho_found_symbol_cb symbol_cb, void *context);
plcrash_error_t plcrash_async_macho_find_symbol_by_name (plcrash_async_macho_t *image, const char *symbol, uintptr_t *pc);

plcrash_error_t plcrash_async_macho_symtab_reader_init (plcrash_async_macho_symtab_reader_t *reader, plcrash_async_macho_t *image);
plcrash_async_macho_symtab_entry_t plcrash_async_macho_symtab_reader_read (plcrash_async_macho_symtab_reader_t *reader, void *symtab, uint32_t index);
uint32_t plcrash_async_macho_symtab_reader_indirect (plcrash_async_macho_symtab_reader_t *reader, uint32_t indirect_idx);
const char *plcrash_async_macho_symtab_reader_symbol_name (plcrash_async_macho_symtab_reader_t *reader, uint32_t n_strx);
void plcrash_async_macho_symtab_reader_free (plcrash_async_macho_symtab_reader_t *reader);

void plcrash_async_macho_mapped_segment_free (pl_async_macho_mapped_segment_t *segment);

void plcrash_nasync_macho_free (plcrash_async_macho_t *image);

/**
 * @}
 */
    
#ifdef __cplusplus
}
#endif

#endif /* PLCRASH_ASYNC_MACHO_IMAGE_H */
