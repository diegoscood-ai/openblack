/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <span>
#include <string>
#include <vector>

#include "VideoDecoder.h"

struct AVCodecContext;
struct AVPacket;
struct AVFrame;

/// The Bink 1 picture decoder of the port (milestone V5): what binkw32.dll 1.0w does for LHVideoPlayer, BinkDoFrame
/// (0x8450FE) and the colours of BinkCopyToBuffer (0x845146), from FFmpeg's libavcodec bink decoder (LGPL-2.1-or-later;
/// vcpkg-overlay-ports/ffmpeg trims FFmpeg to it) fed with BikFile's packets, then binkw32's own YUV -> RGB
/// (BinkYuv.h). Bit-exact with binkw32.dll on the 55 golden frames of the five videos of the game, 555 and 565
/// (dev\_scratch\asistente\video\ffmpeg\README.md; wiki video.md).
///
/// - The codec gets what libavformat/bink.c would give it: the codec tag "BIK" + the revision, the picture size, and
///   the 4 bytes of the header's video flags as extradata. No libavformat: BikFile is the demuxer.
/// - Bink 1 frames are deltas of the one before. DecodeNext(i) in order decodes one packet. Any other `i` (BinkGoto:
///   tips.bik 0x5F3D5F, logo.bik 0x642A2C, all key frames) starts again from the last key frame <= i
///   (avcodec_flush_buffers, then the packets up to i), unless going forward with no key frame in between.
namespace openblack::video
{

class FfmpegDecoder final: public IVideoDecoder
{
public:
	FfmpegDecoder() = default;
	FfmpegDecoder(const FfmpegDecoder&) = delete;
	FfmpegDecoder& operator=(const FfmpegDecoder&) = delete;
	~FfmpegDecoder() override;

	/// libavcodec has the bink decoder (false: FFmpeg was built without it)
	[[nodiscard]] static bool IsAvailable() noexcept;

	bool Open(const BikFile& file) override;
	[[nodiscard]] std::span<const uint8_t> DecodeNext(uint32_t index) override;

	/// Why Open or the last DecodeNext failed
	[[nodiscard]] const std::string& GetError() const { return _error; }

private:
	void Close() noexcept;
	/// avcodec_send_packet + avcodec_receive_frame of frame `index`'s video data. False: the frame failed
	bool DecodePacket(uint32_t index);

	const BikFile* _file {nullptr};
	AVCodecContext* _codec {nullptr};
	AVPacket* _packet {nullptr};
	AVFrame* _frame {nullptr};
	uint32_t _next {0}; ///< the frame the codec decodes next (BINK.FrameNum - 1)
	std::vector<uint8_t> _rgba;
	std::string _error;
};

} // namespace openblack::video
