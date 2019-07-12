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

#include <algorithm>
#include <errno.h>
#include <iostream>
#include <vector>

#include "vcl/KeyFrameParser.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/log.h>
}

using namespace VCL;

/*  *********************** */
/*    KEY_FRAME_PARSER      */
/*  *********************** */
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

/*  *********************** */
/*    KEY_FRAME_DECODER     */
/*  *********************** */
int KeyFrameDecoder::init_decoder(void) {
	int ret = 0;

	_ctx.fmt_context = avformat_alloc_context();
	ret = avformat_open_input(&_ctx.fmt_context,
							  _filename.c_str(), NULL, NULL);
	if (ret != 0)
		return on_error(ret, _filename);

	ret = avformat_find_stream_info(_ctx.fmt_context, NULL);
	if (ret != 0)
		return on_error(ret);

	AVCodecParameters *codec = NULL;
	for (unsigned i = 0; i < _ctx.fmt_context->nb_streams && !codec; i++) {
		if (_ctx.fmt_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			codec = _ctx.fmt_context->streams[i]->codecpar;
			_ctx.stream_index = i;
		}
	}

	if (!codec)
		return on_error(AVERROR_ENCODER_NOT_FOUND, "No video codec");

	if (codec->codec_id != AV_CODEC_ID_H264)
		return on_error(AVERROR_EXTERNAL, "Input is not encoded with H264");

	_ctx.byte_stream_format = (codec->bit_rate) ?
							   H264Format::AVCC : H264Format::AnnexB;

	const struct AVCodec *codec_ptr = avcodec_find_decoder(codec->codec_id);
	if (!codec_ptr)
		return on_error(AVERROR_DECODER_NOT_FOUND, "No video codec");

	_ctx.codec_context = avcodec_alloc_context3(codec_ptr);
	if (!_ctx.codec_context)
		return on_error(AVERROR_DECODER_NOT_FOUND, "Failed to allocate alloc_context");

	ret = avcodec_open2(_ctx.codec_context, codec_ptr, NULL);
	if (ret < 0)
		return on_error(ret);

	return ret;
}

int KeyFrameDecoder::init_bsf(void){
	int ret = 0;
	const AVBitStreamFilter *bsf;

	bsf = av_bsf_get_by_name("h264_mp4toannexb");
	if (!bsf)
		return on_error(AVERROR_BSF_NOT_FOUND, "av_bsf_get_by_name");

	ret = av_bsf_alloc(bsf, &_ctx.bsf_context);
	if (ret != 0)
		return on_error(ret, "av_bsf_alloc");

	AVRational time_base;
	AVCodecParameters *codec;

	time_base = _ctx.fmt_context->streams[_ctx.stream_index]->time_base;
	codec     = _ctx.fmt_context->streams[_ctx.stream_index]->codecpar;

	ret = avcodec_parameters_copy(_ctx.bsf_context->par_in, codec);
	if (ret < 0)
		return on_error(ret, "avcodec_parameters_copy");

	_ctx.bsf_context->time_base_in = time_base;

	ret = av_bsf_init(_ctx.bsf_context);
	if (ret != 0)
		return on_error(ret, "av_bsf_init");

	return 0;
}

int KeyFrameDecoder::decode_interval(KeyFrame start, KeyFrame end,
                                     const std::vector<uint64_t> &frames) {

	AVPacket *pkt = av_packet_alloc();
	if (!pkt)
		return on_error(AVERROR_EXTERNAL, "av_packet_alloc");

	AVFrame *current_frame = av_frame_alloc();
	if (!current_frame)
		return on_error(AVERROR_EXTERNAL, "av_frame_alloc");

	int ret = 0;
	if (_ctx.byte_stream_format == H264Format::AVCC) {
		ret = av_seek_frame(_ctx.fmt_context, _ctx.stream_index, start.idx,
                            AVSEEK_FLAG_FRAME);
	}
	else {
		ret = av_seek_frame(_ctx.fmt_context, _ctx.stream_index, start.base,
                            AVSEEK_FLAG_BYTE);
	}

	if (ret != 0)
		return on_error(ret, "av_seek_frame");

	avcodec_flush_buffers(_ctx.codec_context);

	for (auto idx = start.idx; idx <= end.idx; ) {
		do {
			ret = av_read_frame(_ctx.fmt_context, pkt);
			if (ret == AVERROR_EOF)
				return on_error(AVERROR_EOF, "Encountered EOF while decoding");
		} while (pkt->stream_index != _ctx.stream_index);

		if (_ctx.byte_stream_format != H264Format::AnnexB)
		{
			ret = av_bsf_send_packet(_ctx.bsf_context, pkt);
			if (ret != 0)
				return on_error(ret, "av_bsf_send_packet");

			ret = av_bsf_receive_packet(_ctx.bsf_context, pkt);
			if (ret == AVERROR(EAGAIN))
				continue;
			else if (ret < 0)
				return on_error(AVERROR(ret), "av_bsf_receive_packet");
		}

		ret = avcodec_send_packet(_ctx.codec_context, pkt);
		if (ret < 0)
			return on_error(AVERROR(ret), "Error sending a packet for decoding");

		ret = avcodec_receive_frame(_ctx.codec_context, current_frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			continue;
		else if (ret < 0)
			return on_error(ret, "avcodec_receive_frame");
		else if (ret == 0) {
			auto found = std::find(frames.begin(), frames.end(), idx);
			if (found != frames.end()) {
				AVFrame *frame = av_frame_clone(current_frame);
				_frame_list.push_back({.frame = frame, .idx = idx});
			}
		}
		++idx;
	}

	av_frame_free(&current_frame);

	av_packet_unref(pkt);

	return 0;
}

int KeyFrameDecoder::populate_interval_map(
	                               const std::vector<KeyFrame> &key_frames,
	                               const std::vector<uint64_t> &frames)
{
	// TODO: check whether segment tree makes sense here

	std::vector<KeyFrame> sorted_frame_list(key_frames);

	if (sorted_frame_list.empty() || frames.empty())
		return on_error(AVERROR_EXTERNAL,
                        "Either key frame or query frame list is empty");

	std::sort(sorted_frame_list.begin(), sorted_frame_list.end(),
		      [&](KeyFrame l, KeyFrame r) { return l.idx < r.idx; });

	for (auto i = 0; i < sorted_frame_list.size() - 1; ++i) {

		FrameInterval interval = {.start = sorted_frame_list[i],
		                          .end   = sorted_frame_list[i+1]};

		std::vector<uint64_t> interval_frames;

		for (auto &f : frames) {
			if (f >= interval.start.idx && f < interval.end.idx)
				interval_frames.push_back(f);
		}

		if (!interval_frames.empty())
			_interval_map.push_back(std::make_pair(interval, interval_frames));
	}

	return 0;
}

int KeyFrameDecoder::on_error (int errnum, std::string opt) {
	char errbuf[128];

	int ret = av_strerror(errnum, errbuf, sizeof(errbuf));
	if (ret != 0)
		sprintf(errbuf, "unknown ffmpeg error");

	std::string cause = "";
	if (!opt.empty())
		cause += (opt + ": ");

	std::cerr << "*** " << cause << errbuf << std::endl;
	std::cerr << "*** Key frame detection failed" << std::endl;

	return errnum;
}

void KeyFrameDecoder::context_cleanup(void) {
	for (auto &f : _frame_list) {
		av_frame_free(&f.frame);
	}
	if (_ctx.fmt_context) {
		avformat_close_input(&_ctx.fmt_context);
		avformat_free_context(_ctx.fmt_context);
	}
	if (_ctx.codec_context) {
		avcodec_close(_ctx.codec_context);
	}
	if (_ctx.bsf_context) {
		av_bsf_free(&_ctx.bsf_context);
	}
}

void KeyFrameDecoder::set_filename(std::string filename) {
	_filename = filename;
}

int KeyFrameDecoder::init(void) {
	if (_filename.empty()) {
		std::cerr << "No filename" << std::endl;
		return -1;
	}

	int ret = init_decoder();
	if (ret != 0) {
		context_cleanup();
		return -1;
	}

	ret = init_bsf();
	if (ret != 0) {
		context_cleanup();
		return -1;
	}

	return 0;
}

int KeyFrameDecoder::set_interval_map(
                     const std::vector<KeyFrame> &key_frames,
                     const std::vector<uint64_t> &frames) {

	return populate_interval_map(key_frames, frames);

}

int KeyFrameDecoder::decode(void) {
	int ret;

	for (auto interval : _interval_map) {
		ret = decode_interval(interval.first.start, interval.first.end,
						      interval.second);
		if (ret != 0)
			return ret;
	}

	return 0;
}

KeyFrameDecoder::~KeyFrameDecoder() {
	context_cleanup();
}

