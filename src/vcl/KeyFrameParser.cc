/**
 * @file   KeyFrameParser.cc
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

#include <iostream>
#include <vector>
#include <errno.h>

#include "vcl/KeyFrameParser.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/log.h>
}

using namespace VCL;

int KeyFrameParser::init_stream(void) {

	int ret = 0;

	_tctx.fmt_context = avformat_alloc_context();
	ret = avformat_open_input(&_tctx.fmt_context,
							  _filename.c_str(), NULL, NULL);
	if (ret != 0) {
		return on_error(ret, _filename);
	}

	ret = avformat_find_stream_info(_tctx.fmt_context, NULL);
	if (ret != 0) {
		return on_error(ret);
	}

	AVCodecParameters *codec = NULL;
	AVRational time_base;
	for (unsigned i = 0; i < _tctx.fmt_context->nb_streams && !codec; i++) {
		if (_tctx.fmt_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			codec     = _tctx.fmt_context->streams[i]->codecpar;
			time_base = _tctx.fmt_context->streams[i]->time_base;

			_tctx.stream_index = i;
		}
	}

	if (!codec) {
		return on_error(AVERROR_ENCODER_NOT_FOUND, "No video codec");
	}
	else if (codec->codec_id != AV_CODEC_ID_H264) {
		return on_error(AVERROR_EXTERNAL, "Input is not a H264 stream");
	}

	return ret;
}

int KeyFrameParser::fill_frame_list(KeyFrameList& frame_list) {

	AVPacket *pkt = NULL;

	pkt = av_packet_alloc();
	if (!pkt) {
		return on_error(AVERROR_EXTERNAL, "av_packet_alloc");
	}

	int success = 0;
	int ret     = 0;
	uint64_t frame_idx = 0;
	while ((success = av_read_frame(_tctx.fmt_context, pkt)) != AVERROR_EOF) {

		if (pkt->stream_index != _tctx.stream_index)
			continue;

		if (pkt->flags & AV_PKT_FLAG_KEY) {
			KeyFrame frame = {.idx = frame_idx, .base = pkt->pos,
                              .len = pkt->size };
			frame_list.push_back(frame);
		}

		frame_idx++;
	}

	av_packet_unref(pkt);

	return 0;
}

int KeyFrameParser::on_error (int errnum, std::string opt) {
	char errbuf[128];

	int ret = av_strerror(errnum, errbuf, sizeof(errbuf));
	if (ret != 0) {
		sprintf(errbuf, "unknown ffmpeg error");
	}

	std::string cause = "";
	if (!opt.empty()) {
		cause += (opt + ": ");
	}

	std::cerr << "*** " << cause << errbuf << std::endl;
	std::cerr << "*** Key frame detection failed" << std::endl;

	return errnum;
}

void KeyFrameParser::context_cleanup(void) {
	if (_tctx.fmt_context) {
		avformat_close_input(&_tctx.fmt_context);
		avformat_free_context(_tctx.fmt_context);
	}
}

void KeyFrameParser::set_filename(std::string filename) {
	_filename = filename;
}

int KeyFrameParser::init(void) {

	if (_filename.empty()) {
		std::cerr << "No filename" << std::endl;
		return -1;
	}

	int ret = init_stream();
	if (ret != 0) {
		context_cleanup();
		return -1;
	}

	av_log_set_level(AV_LOG_QUIET);
	return 0;
}

int KeyFrameParser::parse(KeyFrameList& frame_list) {

	int ret = fill_frame_list(frame_list);
	if (ret != 0) {
		context_cleanup();
		return -1;
	}

	return 0;
}

KeyFrameParser::~KeyFrameParser() {
	context_cleanup();
}

