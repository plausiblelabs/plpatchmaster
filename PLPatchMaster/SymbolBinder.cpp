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

#include "SymbolBinder.hpp"

namespace patchmaster {

/**
 * Read a ULEB128 value from @a address.
 *
 * @param location The location from which the value should be read.
 * @param size On return, will be set to the total size of the decoded LEB128 value in bytes.
 *
 * This implementation was extracted from the PLCrashReporter DWARF code.
 */
uint64_t read_uleb128 (const void *location, std::size_t *size) {
    unsigned int shift = 0;
    size_t position = 0;
    
    uint64_t result = 0;
    for (const uint8_t *p = (const uint8_t *) location ;; p++) {
        /* LEB128 uses 7 bits for the number, the final bit to signal completion */
        uint8_t byte = *p;
        result |= ((uint64_t) (byte & 0x7f)) << shift;
        shift += 7;
        
        /* This is used to track length, so we must set it before
         * potentially terminating the loop below */
        position++;
        
        /* Check for terminating bit */
        if ((byte & 0x80) == 0)
            break;
        
        /* Check for a ULEB128 larger than 64-bits */
        if (shift >= 64) {
            PMFatal("Invalid DYLD info: ULEB128 is larger than the maximum supported size of 64 bits!");
        }
    }
    
    *size = position;
    return result;
}

/**
 * Read a SLEB128 value from @a location within @a mobj.
 *
 * @param location The location from which the value should be read.
 * @param size On return, will be set to the total size of the decoded LEB128 value in bytes.
 *
 * This implementation was extracted from the PLCrashReporter DWARF code.
 */
int64_t read_sleb128 (const void *location, std::size_t *size) {
    unsigned int shift = 0;
    size_t position = 0;
    int64_t result = 0;
    
    const uint8_t *p;
    for (p = (const uint8_t *) location ;; p++) {
        /* LEB128 uses 7 bits for the number, the final bit to signal completion */
        uint8_t byte = *p;
        result |= ((uint64_t) (byte & 0x7f)) << shift;
        shift += 7;
        
        /* This is used to track length, so we must set it before
         * potentially terminating the loop below */
        position++;
        
        /* Check for terminating bit */
        if ((byte & 0x80) == 0)
            break;
        
        /* Check for a SLEB128 larger than 64-bits */
        if (shift >= 64) {
            PMFatal("Invalid DYLD info: SLEB128 is larger than the maximum supported size of 64 bits!");
        }
    }
    
    /* Sign bit is 2nd high order bit */
    if (shift < 64 && (*p & 0x40))
        result |= -(1ULL << shift);
    
    *size = position;
    return result;
}
    
    
/**
 * Analyze an in-memory Mach-O image.
 *
 * @param path The image path.
 * @param header The image header.
 */
LocalImage LocalImage::Analyze (const std::string &path, const pl_mach_header_t *header) {
    using namespace std;
    
    /* Image slide */
    intptr_t vm_slide = 0;
    
    /* Collect the segment and library lists, saving the __LINKEDIT info and vm_slide */
    auto segments = std::make_shared<vector<const pl_segment_command_t *>>();
    auto libraries = std::make_shared<vector<const std::string>>();
    pl_segment_command_t *linkedit = nullptr;
    
    const uint8_t *cmd_ptr = (const uint8_t *) header + sizeof(*header);
    for (uint32_t cmd_idx = 0; cmd_idx < header->ncmds; cmd_idx++) {
        auto cmd = (const struct load_command *) cmd_ptr;
        cmd_ptr += cmd->cmdsize;
        
        switch (cmd->cmd) {
            case PL_LC_SEGMENT: {
                auto segment = (pl_segment_command_t *) cmd;
                
                /* Use the actual load address of the __TEXT segment to calculate the dyld slide */
                if (strcmp(segment->segname, SEG_TEXT) == 0) {
                    uintptr_t load_addr = (uintptr_t) header;
                    if (segment->vmaddr < load_addr) {
                        vm_slide = load_addr - segment->vmaddr;
                    } else if (segment->vmaddr > load_addr) {
                        vm_slide = -((intptr_t) (segment->vmaddr - load_addr));
                    } else {
                        vm_slide = 0;
                    }
                } else if (strcmp(segment->segname, SEG_LINKEDIT) == 0) {
                    linkedit = segment;
                }
                
                /* For the purposes of indexing segments, dyld ignores zero-length segments */
                if (segment->vmsize > 0)
                    segments->push_back(segment);
                break;
            }
                
            case LC_LOAD_DYLIB:
            case LC_LOAD_WEAK_DYLIB:
            case LC_LOAD_UPWARD_DYLIB:
            case LC_REEXPORT_DYLIB:
            {
                auto dylib_cmd = (struct dylib_command *) cmd;
                
                /* Fetch the library path */
                const char *name = (const char *) (((const char *) cmd) + dylib_cmd->dylib.name.offset);
                libraries->push_back(name);
            }
        }
    }

    /* Save references to all dyld bind opcode streams */
    auto bindOpcodes = std::make_shared<vector<const bind_opstream>>();
    
    cmd_ptr = (const uint8_t *) header + sizeof(*header);
    for (uint32_t cmd_idx = 0; cmd_idx < header->ncmds; cmd_idx++) {
        auto cmd = (const struct load_command *) cmd_ptr;
        cmd_ptr += cmd->cmdsize;
        
        switch (cmd->cmd) {
            case LC_DYLD_INFO:
            case LC_DYLD_INFO_ONLY: if (linkedit != nullptr) {
                auto info = (const dyld_info_command *) cmd;
                uintptr_t linkedit_base = (linkedit->vmaddr + vm_slide) - linkedit->fileoff;
                
                if (info->bind_size != 0)
                    bindOpcodes->push_back(bind_opstream((const uint8_t *) (linkedit_base + info->bind_off), (size_t) info->bind_size, false));
                
                if (info->weak_bind_size != 0)
                    bindOpcodes->push_back(bind_opstream((const uint8_t *) (linkedit_base + info->weak_bind_off), (size_t) info->weak_bind_size, false));
                
                if (info->lazy_bind_size != 0)
                    bindOpcodes->push_back(bind_opstream((const uint8_t *) (linkedit_base + info->lazy_bind_off), (size_t) info->lazy_bind_size, true));
            }
                
            default:
                break;
        }
    }
    
    return LocalImage(path, header, vm_slide, libraries, segments, bindOpcodes);
}

/**
 * Evaluate all available dyld bind opcodes, passing all resolved bindings to @a binder.
 *
 * @param binder The function to be called with resolved symbol bindings.
 *
 * @return Returns true on success, or false if the opcode stream references invalid segment or image addresses.
 */
void LocalImage::rebind_symbols (const bind_fn &binder) {
    for (auto &&opcodes : *_bindOpcodes)
        evaluate_bind_opstream(opcodes, binder);
}

/**
 * Evaluate the given opcode stream, passing all resolved bindings to @a binder.
 *
 * @param opcodes The opcode stream to be evaluated.
 * @param binder The function to be called with resolved symbol bindings.
 *
 * @return Returns true on success, or false if the opcode stream references invalid segment or image addresses.
 */
void LocalImage::evaluate_bind_opstream (const bind_opstream &opcodes, const bind_fn &binder) {
    using namespace std;
    
    /* dylib path from which the symbol will be resolved, or an empty string if unspecified or flat binding. */
    std::string sym_image("");
    
    /* buffer used to hold an allocated image path, if any */
    std::vector<char> sym_image_buffer;
    
    /* bind type (one of BIND_TYPE_POINTER, BIND_TYPE_TEXT_ABSOLUTE32, or BIND_TYPE_TEXT_PCREL32) */
    uint8_t bind_type = BIND_TYPE_POINTER;
    
    /* symbol name */
    const char *sym_name = "";
    
    /* symbol flags (one of BIND_SYMBOL_FLAGS_WEAK_IMPORT, BIND_SYMBOL_FLAGS_NON_WEAK_DEFINITION) */
    uint8_t sym_flags = 0;
    
    /* A value to be added to the resolved symbol's address before binding. */
    int64_t addend = 0;
    
    /* The actual in-memory bind target address. */
    uintptr_t bind_address = 0;
    
    /*
     * Check our patch table for this symbol; if found, try to apply
     */
    auto handle_bind = [&]() {
        // TODO - Can we handle the other types?
        if (bind_type != BIND_TYPE_POINTER)
            return;
        
        /* Let our caller perform a bind */
        binder(SymbolName(sym_image, sym_name), (uintptr_t *) bind_address, addend);
    };
    
    /* Given an index into our reference libraries, update the `sym_image` state */
    auto set_current_image = [&](uint64_t image_idx) {
        if (image_idx > _libraries->size()) {
            PMFatal("dyld bind opcode in '%s' references invalid image index %" PRIu64, _path.c_str(), image_idx);
            return;
        }
        
        /* `0` is a special index referencing the current image */
        if (image_idx == 0) {
            sym_image = _path;
        } else {
            sym_image = _libraries->at(image_idx - 1);
        }
    };
    
    bind_opstream ops = opcodes;
    while (!ops.isEmpty()) {
        uint8_t opcode = ops.opcode();
        switch (opcode) {
            case BIND_OPCODE_DONE:
                return;
                
            case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM: {
                set_current_image(ops.immd());
                break;
            }
                
            case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB: {
                set_current_image(ops.uleb128());
                break;
            }
                
            case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
                switch (ops.signed_immd()) {
                        /* Enable flat resolution */
                    case BIND_SPECIAL_DYLIB_FLAT_LOOKUP:
                        sym_image = "";
                        break;
                        
                        /* Fetch the path of the main executable */
                    case BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE: {
                        uint32_t buflen = (uint32_t) sym_image_buffer.size();
                        while (_NSGetExecutablePath(&sym_image_buffer[0], &buflen) == -1) {
                            sym_image_buffer.resize(buflen);
                        }
                        
                        sym_image = string(&sym_image_buffer[0]);
                        break;
                    }
                        
                        /* Use our own path */
                    case BIND_SPECIAL_DYLIB_SELF:
                        sym_image = _path.c_str();
                        break;
                }
                
                break;
                
            case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
                /* Flags are supplied as an immediate value. */
                sym_flags = ops.immd();
                
                /* Symbol name is defined inline. */
                sym_name = ops.cstring();
                break;
                
            case BIND_OPCODE_SET_TYPE_IMM:
                bind_type = ops.immd();
                break;
                
            case BIND_OPCODE_SET_ADDEND_SLEB:
                addend = ops.sleb128();
                break;
                
            case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB: {
                uint8_t segment_idx = ops.immd();
                if (segment_idx >= _segments->size())
                    PMFatal("dyld BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB in '%s' references invalid segment index %" PRIu8, _path.c_str(), segment_idx);
                
                /* Compute the in-memory address from the segment reference */
                const pl_segment_command_t *segment = _segments->at(segment_idx);
                bind_address = segment->vmaddr + _vmaddr_slide;
                bind_address += ops.uleb128();
                break;
            }
                
            case BIND_OPCODE_ADD_ADDR_ULEB:
                bind_address += ops.uleb128();
                break;
                
            case BIND_OPCODE_DO_BIND:
                /* Perform the bind */
                handle_bind();
                
                /* This implicitly advances the current bind address by the pointer width */
                bind_address += sizeof(uintptr_t);
                break;
                
            case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
                /* Perform the bind */
                handle_bind();
                
                /* Advance the bind address */
                bind_address += ops.uleb128();
                break;
                
            case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
                /* Perform the bind */
                handle_bind();
                
                /* Immediate offset scaled by the native pointer width */
                bind_address += ops.immd() * sizeof(uintptr_t) + sizeof(uintptr_t);
                break;
                
            case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB: {
                /* Fetch the number of addresses at which this symbol should be bound */
                uint64_t count = ops.uleb128();
                
                /* Fetch the number of bytes to skip between each binding */
                uint64_t skip = ops.uleb128();
                
                for (uint64_t i = 0; i < count; i++) {
                    /* Perform the bind */
                    handle_bind();
                    
                    /* Advance by the requested skip */
                    bind_address += skip + sizeof(uintptr_t);
                }
                break;
            }
                
            default:
                PMFatal("Unhandled opcode: %hhx", opcode);
                break;
        }
    }
};


}