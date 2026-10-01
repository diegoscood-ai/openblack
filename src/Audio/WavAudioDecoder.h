/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <dr_wav.h>

#include "AudioDecoderInterface.h"

namespace openblack::audio
{

class WavAudioDecoder final: public AudioDecoderInterface
{
public:
	WavAudioDecoder() = default;
	WavAudioDecoder(const WavAudioDecoder&) = delete;
	WavAudioDecoder& operator=(const WavAudioDecoder&) = delete;
	~WavAudioDecoder();
	bool Open(const std::vector<uint8_t>& buffer) override;
	void Read(std::vector<int16_t>& buffer) override;
	[[nodiscard]] ChannelLayout GetChannelLayout() override;
	/// The decoded rate in Hz (0 before a successful Open)
	[[nodiscard]] int GetSampleRate() const;

private:
	drwav _wav {};
	bool _open {false};
};

} // namespace openblack::audio
