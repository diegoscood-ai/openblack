/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <span>
#include <vector>

#include "BikFile.h"

/// The picture decoder behind the video player: what binkw32.dll does for LHVideoPlayer (BinkOpen fn_00844E70
/// 0x844E8C, BinkDoFrame + BinkNextFrame in DecodeNextFrame fn_008450B0 0x8450E3 / 0x8450FE). The 16-bit copy
/// (BinkCopyToBuffer 0x845146) is not the decoder's: the player does it with graphics::rgb16.
namespace openblack::video
{

class IVideoDecoder
{
public:
	virtual ~IVideoDecoder() = default;

	/// BinkOpen + BinkGetSummary: get ready to decode `file` (it outlives the decoder). False: the film cannot be
	/// decoded (LHVideoPlayer keeps +0x1C = NULL, 0x844E92..0x844EA8)
	virtual bool Open(const BikFile& file) = 0;
	/// BinkDoFrame of frame `index` (0, 1, 2, ... in order: Bink 1 frames are deltas of the one before): the picture as
	/// RGBA8, width * height * 4 bytes, valid up to the next call. Empty: the frame failed (BinkDoFrame != 0,
	/// 0x845104), and the player keeps the picture it had
	[[nodiscard]] virtual std::span<const uint8_t> DecodeNext(uint32_t index) = 0;
};

/// No picture yet (the Bink decoder is milestone V5): every frame is opaque black, so the player behaves as the
/// original (pause, wide screen, fade, skip) over a black screen
class NullVideoDecoder final: public IVideoDecoder
{
public:
	bool Open(const BikFile& file) override
	{
		_frames = file.FrameCount();
		_pixels.assign(static_cast<size_t>(file.Width()) * file.Height() * 4, 0);
		for (size_t i = 3; i < _pixels.size(); i += 4)
		{
			_pixels[i] = 0xFF;
		}
		return true;
	}

	[[nodiscard]] std::span<const uint8_t> DecodeNext(uint32_t index) override
	{
		if (index >= _frames)
		{
			return {};
		}
		return _pixels;
	}

private:
	uint32_t _frames {0};
	std::vector<uint8_t> _pixels;
};

} // namespace openblack::video
