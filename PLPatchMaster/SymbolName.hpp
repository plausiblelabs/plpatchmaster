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

namespace patchmaster {
    /**
     * A single-level or two-level namespaced symbol reference.
     */
    class SymbolName {
    public:
        /**
         * Construct a new symbol name.
         *
         * @param image The absolute or relative path of the image that exports this symbol, or an empty path to signify
         * single-level lookup.
         * @param symbol The symbol name.
         */
        SymbolName (const std::string &image, const std::string &symbol) : _image(image), _symbol(symbol) {}
        SymbolName (const std::string &&image, const std::string &&symbol) : _image(std::move(image)), _symbol(std::move(symbol)) {}
        
        /** Return the absolute or relative path of the image that exports this symbol. If the path is empty, single-level
         * namespacing is assumed. */
        const std::string &image () const { return _image; }
        
        /** Return the symbol name. */
        const std::string &symbol () const { return _symbol; }
        
        /**
         * Return true if this symbol name matches the provided name.
         */
        bool match (const SymbolName &other) const {
            /* If symbol names don't match, there's nothing else to test. */
            if (other._symbol != _symbol)
                return false;
            
            /* If either image is zero-length, they'll match on the first matching symbol regardless of the image. */
            if (other._image.length() == 0 || _image.length() == 0)
                return true;
            
            /* Check for an exact image match */
            if (other._image == _image)
                return true;
            
            /* If either path is relative, perform substring matching */
            if (_image[0] != '/' && other._image.length() >= _image.length()) {
                return other._image.compare (other._image.length() - _image.length(), _image.length(), _image) == 0;
            } else if (other._image[0] != '/' && _image.length() >= other._image.length()) {
                return _image.compare (_image.length() - other._image.length(), other._image.length(), other._image) == 0;
            }
            
            /* No match */
            return false;
        }
        
    private:
        /** Image, or empty string */
        const std::string _image;
        
        /** Symbol name. */
        const std::string _symbol;
    };
} /* namespace patchmaster */