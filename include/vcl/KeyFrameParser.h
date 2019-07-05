/**
 * @file   KeyFrameParser.h
 *
 * @section LICENSE
 *
 * The MIT License
 *
 * @copyright Copyright (c) 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#pragma once

#include <string>
#include <vector>

extern "C"
{
#include <libavformat/avformat.h>
}

namespace VCL {

    struct KeyFrame {
        uint64_t idx;
	    int64_t  base;
	    int      len;
    };

    using KeyFrameList = std::vector<KeyFrame>;

    class KeyFrameParser {

    private:

        struct TraceContext {
            AVFormatContext  *fmt_context;
            unsigned stream_index;
        };

        TraceContext _tctx;
        std::string  _filename;
        KeyFrameList _frame_list;

        int init_stream(void);
        int fill_frame_list(KeyFrameList& frame_list);
        int on_error(int errnum, const std::string opt = "");

        void context_cleanup (void);

    public:

        KeyFrameParser() {} ;
        KeyFrameParser(std::string filename): _filename(filename) {};
        KeyFrameParser(KeyFrameParser&) = delete;
        ~KeyFrameParser();

        void set_filename(std::string filename);

        int init(void);
        int parse(KeyFrameList& frame_list);
    };
}