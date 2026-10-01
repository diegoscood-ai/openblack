/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <dr_mp3.h>

#include "AudioDecoderInterface.h"

namespace openblack::audio
{

class MpegAudioDecoder final: public AudioDecoderInterface
{
public:
	MpegAudioDecoder() = default;
	MpegAudioDecoder(const MpegAudioDecoder&) = delete;
	MpegAudioDecoder& operator=(const MpegAudioDecoder&) = delete;
	~MpegAudioDecoder();
	bool Open(const std::vector<uint8_t>& buffer) override;
	void Read(std::vector<int16_t>& buffer) override;
	[[nodiscard]] ChannelLayout GetChannelLayout() override;
	/// The decoded rate in Hz (0 before a successful Open)
	[[nodiscard]] int GetSampleRate() const;

private:
	drmp3 _mp3 {};
	bool _open {false};
};

} // namespace openblack::audio
