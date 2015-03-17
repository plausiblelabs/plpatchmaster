/*
 * Author: Landon Fuller <landon@landonf.org>
 *
 * Copyright (c) 2015 Landon Fuller <landon@landonf.org>.
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

#pragma once

#include "PMLog.h"

#include <mach-o/loader.h>
#include <mach-o/dyld.h>

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <dlfcn.h>

#include <vector>
#include <map>
#include <string>

#include "SymbolName.hpp"

namespace patchmaster {

/* Architecture-specific Mach-O types and constants */
#ifdef __LP64__
typedef struct mach_header_64 pl_mach_header_t;
typedef struct segment_command_64 pl_segment_command_t;
typedef struct section_64 pl_section_t;
typedef struct nlist_64 pl_nlist_t;
static constexpr uint32_t PL_LC_SEGMENT = LC_SEGMENT_64;
#else
typedef struct mach_header pl_mach_header_t;
typedef struct segment_command pl_segment_command_t;
typedef struct section pl_section_t;
typedef struct nlist pl_nlist_t;
static constexpr uint32_t PL_LC_SEGMENT = LC_SEGMENT;
#endif


uint64_t read_uleb128 (const void *location, std::size_t *size);
int64_t read_sleb128 (const void *location, std::size_t *size);

/**
 * A simple byte-based opcode stream reader.
 *
 * This was adapted from our DWARF opcode evaluation code in PLCrashReporter.
 */
class bind_opstream {
    /** Current position within the op stream */
    const uint8_t *_p;
    
    /** Starting address. */
    const uint8_t *_instr;
    
    /** Ending address. */
    const uint8_t *_instr_max;
    
    /** Current immediate value */
    uint8_t _immd = 0;
    
    /** 
     * If true, this is a lazy opcode section; BIND_OPCODE_DONE is automatically skipped at the end of
     * each entry (the lazy section is written to terminate evaluation after each entry, as each symbol within
     * the lazy section is by dyld on-demand, and is supposed to terminate after resolving one symbol).
     */
    bool _isLazy;

public:
    bind_opstream (const uint8_t *opcodes, const size_t opcodes_len, bool isLazy) : _p(opcodes), _instr(_p), _instr_max(_p + opcodes_len), _isLazy(isLazy) {}
    
    bind_opstream (const bind_opstream &other) : _p(other._p), _instr(other._instr), _instr_max(other._instr_max), _isLazy(other._isLazy) {}
    
    /** Read a ULEB128 value and advance the stream */
    inline uint64_t uleb128 () {
        size_t len;
        uint64_t result = read_uleb128(_p, &len);
        
        _p += len;
        assert(_p <= _instr_max);
        return result;
    }

    /** Read a SLEB128 value and advance the stream */
    inline int64_t sleb128 () {
        size_t len;
        int64_t result = read_sleb128(_p, &len);
        
        _p += len;
        assert(_p <= _instr_max);
        return result;
    }

    /** Skip @a offset bytes. */
    inline void skip (size_t offset) {
        _p += offset;
        assert(_p <= _instr_max);
    }
    
    /** Read a single opcode from the stream. */
    inline uint8_t opcode () {
        assert(_p < _instr_max);
        uint8_t value = (*_p) & BIND_OPCODE_MASK;
        _immd = (*_p) & BIND_IMMEDIATE_MASK;
        _p++;
        
        /* Skip BIND_OPCODE_DONE if it occurs within a lazy binding opcode stream */
        if (_isLazy && *_p == BIND_OPCODE_DONE && !isEmpty())
            skip(1);
        
        return value;
    };

    /** Return the current stream position. */
    inline const uint8_t *position () { return _p; };
    
    /** Return true if there are no additional opcodes to be read. */
    inline bool isEmpty () { return _p >= _instr_max; }

    /** Read a NUL-terminated C string from the stream, advancing the current position past the string. */
    inline const char *cstring () {
        const char *result = (const char *) _p;
        skip(strlen(result) + 1);
        return result;
    }
    
    /** Return the immediate value from the last opcode */
    inline uint8_t immd () { return _immd; }
    
    /** Return the signed representation of immd */
    inline int8_t signed_immd () {
        /* All other constants are negative */
        if (immd() == 0)
            return 0;
        
        /* Sign-extend the immediate value */
        return (~BIND_IMMEDIATE_MASK) | (immd() & BIND_IMMEDIATE_MASK);
    }

};

/**
 * An in-memory Mach-O image.
 */
class LocalImage {
private:
    /**
     * Construct a new local image.
     */
    LocalImage (
        const std::string &path,
        const pl_mach_header_t *header,
        const intptr_t vmaddr_slide,
        std::shared_ptr<std::vector<const std::string>> &libraries,
        std::shared_ptr<std::vector<const pl_segment_command_t *>> &segments,
        std::shared_ptr<std::vector<const bind_opstream>> &bindings
    ) : _header(header), _vmaddr_slide(vmaddr_slide), _libraries(libraries), _segments(segments), _bindOpcodes(bindings), _path(path) {}

public:
    /** Symbol binding function */
    using bind_fn = std::function<void (const SymbolName &name, uintptr_t *target, int64_t addend)>;

    const std::string &MainExecutablePath ();
    static LocalImage Analyze (const std::string &path, const pl_mach_header_t *header);
    void rebind_symbols (const bind_fn &binder);
    void evaluate_bind_opstream (const bind_opstream &opcodes, const bind_fn &binder);

    
private:
    /** Mach-O image header */
    const pl_mach_header_t *_header;
    
    /** Offset applied when the image was loaded; required to compute in-memory addresses from on-disk VM addresses.. */
    const intptr_t _vmaddr_slide;
    
    /** Linked libraries, indexed by reference order. */
    std::shared_ptr<std::vector<const std::string>> _libraries;
    
    /** Segment commands, indexed by declaration order. */
    std::shared_ptr<std::vector<const pl_segment_command_t *>> _segments;
    
    /** All symbol binding opcodes. */
    std::shared_ptr<std::vector<const bind_opstream>> _bindOpcodes;

    /** Image path */
    const std::string _path;
};

} /* namespace patchmaster */