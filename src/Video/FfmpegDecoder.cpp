/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FfmpegDecoder.h"

#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>
}

#include "BinkYuv.h"

using namespace openblack::video;

namespace
{
std::string AvError(int code)
{
	char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
	av_strerror(code, buffer, sizeof(buffer));
	return buffer;
}

/// libavformat/bink.c read_header: codec_tag = the first 4 bytes of the file, little-endian ("BIKi": 'i' in the top
/// byte, which bink.c decode_init reads as the revision)
constexpr uint32_t CodecTag(char revision) noexcept
{
	return static_cast<uint32_t>('B') | static_cast<uint32_t>('I') << 8 | static_cast<uint32_t>('K') << 16 |
	       static_cast<uint32_t>(static_cast<uint8_t>(revision)) << 24;
}
} // namespace

FfmpegDecoder::~FfmpegDecoder()
{
	Close();
}

bool FfmpegDecoder::IsAvailable() noexcept
{
	return avcodec_find_decoder(AV_CODEC_ID_BINKVIDEO) != nullptr;
}

void FfmpegDecoder::Close() noexcept
{
	av_frame_free(&_frame);
	av_packet_free(&_packet);
	avcodec_free_context(&_codec);
	_file = nullptr;
	_next = 0;
}

bool FfmpegDecoder::Open(const BikFile& file)
{
	Close();
	_error.clear();
	if (!file.IsOpen() || file.Width() == 0 || file.Height() == 0)
	{
		_error = "no film";
		return false;
	}
	const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_BINKVIDEO);
	if (codec == nullptr)
	{
		_error = "libavcodec has no bink decoder";
		return false;
	}
	_codec = avcodec_alloc_context3(codec);
	_packet = av_packet_alloc();
	_frame = av_frame_alloc();
	if (_codec == nullptr || _packet == nullptr || _frame == nullptr)
	{
		Close();
		_error = "out of memory";
		return false;
	}
	_codec->codec_tag = CodecTag(file.Revision());
	_codec->width = static_cast<int>(file.Width());
	_codec->height = static_cast<int>(file.Height());
	_codec->thread_count = 1;
	// ff_get_extradata(s, par, pb, 4): the header's video flags (+36), with the padding libavcodec wants zeroed
	_codec->extradata = static_cast<uint8_t*>(av_mallocz(4 + AV_INPUT_BUFFER_PADDING_SIZE));
	if (_codec->extradata == nullptr)
	{
		Close();
		_error = "out of memory";
		return false;
	}
	const uint32_t flags = file.VideoFlags();
	for (int i = 0; i < 4; ++i)
	{
		_codec->extradata[i] = static_cast<uint8_t>(flags >> (8 * i));
	}
	_codec->extradata_size = 4;
	if (const int ret = avcodec_open2(_codec, codec, nullptr); ret < 0)
	{
		Close();
		_error = "avcodec_open2: " + AvError(ret);
		return false;
	}
	_file = &file;
	_next = 0;
	_rgba.assign(static_cast<size_t>(file.Width()) * file.Height() * 4, 0);
	return true;
}

bool FfmpegDecoder::DecodePacket(uint32_t index)
{
	const auto data = _file->VideoData(index);
	// An empty packet would be the end-of-stream flush for libavcodec
	if (data.empty())
	{
		_error = "empty video packet";
		return false;
	}
	if (const int ret = av_new_packet(_packet, static_cast<int>(data.size())); ret < 0)
	{
		_error = "av_new_packet: " + AvError(ret);
		return false;
	}
	std::memcpy(_packet->data, data.data(), data.size());
	int ret = avcodec_send_packet(_codec, _packet);
	av_packet_unref(_packet);
	if (ret < 0)
	{
		_error = "avcodec_send_packet: " + AvError(ret);
		return false;
	}
	// The bink decoder has no delay: one packet in, one picture out
	ret = avcodec_receive_frame(_codec, _frame);
	if (ret < 0)
	{
		_error = "avcodec_receive_frame: " + AvError(ret);
		return false;
	}
	if (_frame->width != static_cast<int>(_file->Width()) || _frame->height != static_cast<int>(_file->Height()))
	{
		_error = "the picture is not the header's size";
		return false;
	}
	return true;
}

std::span<const uint8_t> FfmpegDecoder::DecodeNext(uint32_t index)
{
	if (_file == nullptr || index >= _file->FrameCount())
	{
		return {};
	}
	if (index != _next)
	{
		// BinkGoto: from the last key frame <= index, unless it is forward with no key frame in between
		uint32_t key = index;
		while (key > 0 && !_file->IsKeyFrame(key))
		{
			--key;
		}
		if (!(index > _next && key <= _next))
		{
			avcodec_flush_buffers(_codec);
			_next = key;
		}
		for (; _next < index; ++_next)
		{
			DecodePacket(_next); // a failed frame keeps the picture before it, as in order
		}
	}
	const bool decoded = DecodePacket(index);
	_next = index + 1;
	if (!decoded)
	{
		return {}; // BinkDoFrame != 0 (0x845104): the player keeps the picture it had
	}
	const bink_yuv::Planes planes {_frame->data[0],
	                               _frame->data[1],
	                               _frame->data[2],
	                               _frame->linesize[0],
	                               _frame->linesize[1],
	                               _frame->linesize[2],
	                               _file->Width(),
	                               _file->Height()};
	bink_yuv::CopyToRgba8(planes, _rgba);
	return _rgba;
}
