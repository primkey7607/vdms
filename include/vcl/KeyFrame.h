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

    /*  *********************** */
    /*    KEY_FRAME_PARSER      */
    /*  *********************** */
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

    /*  *********************** */
    /*    KEY_FRAME_DECODER     */
    /*  *********************** */
    class KeyFrameDecoder {

    private:

        enum class H264Format {
            AVCC   = 0,
            AnnexB = 1
        };

        struct DecoderContext {
            AVBSFContext    *bsf_context;
            AVFormatContext *fmt_context;
            AVCodecContext  *codec_context;
            unsigned stream_index;
            H264Format      byte_stream_format;

            DecoderContext() : bsf_context(NULL), fmt_context(NULL),
                               codec_context(NULL), stream_index(0),
                               byte_stream_format(H264Format::AVCC) {};
        };

        struct FrameInterval {
            KeyFrame start;
            KeyFrame end;
        };

        struct DecodedKeyFrame {
            AVFrame *frame;
            uint64_t idx;
        };

        std::vector<std::pair<FrameInterval, std::vector<uint64_t>>> _interval_map;
        std::vector<DecodedKeyFrame> _frame_list;
        std::string    _filename;
        DecoderContext _ctx;

        int init_decoder(void);
        int init_bsf(void);
        int decode_interval(KeyFrame start, KeyFrame end,
                            const std::vector<uint64_t> &frames);

        int populate_interval_map(const KeyFrameList          &key_frames,
                                  const std::vector<uint64_t> &frames);

        int on_error(int errnum, const std::string opt = "");
        void context_cleanup (void);

    public:
        KeyFrameDecoder() {};
        KeyFrameDecoder(std::string filename): _filename(filename) {};
        KeyFrameDecoder(KeyFrameDecoder&) = delete;
        ~KeyFrameDecoder();

        void set_filename(std::string filename);

        int init(void);
        int set_interval_map(const KeyFrameList          &key_frames,
                             const std::vector<uint64_t> &frames);
        int decode(void);
    };
}