/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WaveBuffers.h"

#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>

#include <spdlog/spdlog.h>

#include "Banks.h"
#include "Device.h"
#include "MpegAudioDecoder.h"
#include "WavAudioDecoder.h"

using namespace openblack::audio;

namespace
{
std::vector<BufferId> g_Buffers;
size_t g_Made = 0;

bool Trace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_AUDIO_TRACE") != nullptr;
	return k_Trace;
}

uint32_t ReadU32(const std::vector<uint8_t>& bytes, size_t at)
{
	uint32_t v = 0;
	std::memcpy(&v, bytes.data() + at, sizeof(v));
	return v;
}

uint16_t ReadU16(const std::vector<uint8_t>& bytes, size_t at)
{
	uint16_t v = 0;
	std::memcpy(&v, bytes.data() + at, sizeof(v));
	return v;
}

bool IsRiff(const std::vector<uint8_t>& bytes)
{
	return bytes.size() >= 12 && std::memcmp(bytes.data(), "RIFF", 4) == 0 && std::memcmp(bytes.data() + 8, "WAVE", 4) == 0;
}

/// The RIFF wave's format tag and its "data" chunk (the chunks are {id[4], u32 size, bytes, a pad byte to even})
struct RiffWave
{
	uint16_t formatTag {0};
	size_t dataOffset {0};
	size_t dataSize {0};
};

std::optional<RiffWave> ParseRiff(const std::vector<uint8_t>& bytes)
{
	RiffWave wave;
	bool haveFormat = false;
	bool haveData = false;
	size_t at = 12;
	while (at + 8 <= bytes.size())
	{
		const std::string_view id(reinterpret_cast<const char*>(bytes.data() + at), 4);
		const size_t size = ReadU32(bytes, at + 4);
		const size_t body = at + 8;
		if (id == "fmt " && size >= 2 && body + 2 <= bytes.size())
		{
			wave.formatTag = ReadU16(bytes, body);
			haveFormat = true;
		}
		else if (id == "data")
		{
			wave.dataOffset = body;
			wave.dataSize = std::min(size, bytes.size() - std::min(body, bytes.size()));
			haveData = true;
		}
		at = body + size + (size & 1);
	}
	if (!haveFormat || !haveData)
	{
		return std::nullopt;
	}
	return wave;
}

template <typename Decoder>
bool DecodeWith(const std::vector<uint8_t>& bytes, wave_buffers::Pcm& out)
{
	Decoder decoder;
	if (!decoder.Open(bytes))
	{
		return false;
	}
	decoder.Read(out.samples);
	out.layout = decoder.GetChannelLayout();
	out.sampleRate = decoder.GetSampleRate();
	return !out.samples.empty();
}
} // namespace

bool wave_buffers::Decode(const Sound& sound, Pcm& out)
{
	out = {};
	// a wave left in its .sad (Sound::waveFile) is read now; the bytes are not kept (the buffer is)
	std::vector<std::vector<uint8_t>> onDemand;
	if (sound.buffer.empty() && !sound.waveFile.empty())
	{
		onDemand.emplace_back();
		if (!banks::ReadWave(sound, onDemand.back()))
		{
			return false;
		}
	}
	for (const auto& bytes : sound.buffer.empty() ? onDemand : sound.buffer)
	{
		Pcm part;
		bool ok = false;
		if (IsRiff(bytes))
		{
			const auto riff = ParseRiff(bytes);
			// WAVE_FORMAT_MPEG 0x50 (and 0x55, MPEG layer III): ACM's MPEG codec in QMixer; dr_mp3 reads layers I..III
			if (riff && (riff->formatTag == 0x50 || riff->formatTag == 0x55))
			{
				const std::vector<uint8_t> data(bytes.begin() + static_cast<std::ptrdiff_t>(riff->dataOffset),
				                                bytes.begin() + static_cast<std::ptrdiff_t>(riff->dataOffset + riff->dataSize));
				ok = DecodeWith<MpegAudioDecoder>(data, part);
			}
			else
			{
				// a .sad sample is a RIFF wave: trying MPEG first let dr_mp3 find frame syncs inside some PCM waves
				// (G_BigSplash_03 decoded to a fraction of a second)
				ok = DecodeWith<WavAudioDecoder>(bytes, part);
			}
		}
		else
		{
			ok = DecodeWith<MpegAudioDecoder>(bytes, part) || DecodeWith<WavAudioDecoder>(bytes, part);
		}
		if (!ok)
		{
			continue;
		}
		if (out.samples.empty())
		{
			out.layout = part.layout;
			out.sampleRate = part.sampleRate;
		}
		out.samples.insert(out.samples.end(), part.samples.begin(), part.samples.end());
	}
	return !out.samples.empty();
}

BufferId wave_buffers::Get(Sound& sound)
{
	if (sound.bufferId != 0)
	{
		return sound.bufferId;
	}
	if (!device::IsOpen())
	{
		return 0;
	}
	Pcm pcm;
	if (!Decode(sound, pcm))
	{
		if (auto logger = spdlog::get("audio"))
		{
			SPDLOG_LOGGER_ERROR(logger, "Unable to decode sound {} (format {:#x})", sound.name, sound.waveFormat);
		}
		return 0;
	}
	const int rate = pcm.sampleRate > 0 ? pcm.sampleRate : sound.sampleRate;
	const BufferId id = device::CreateBuffer(pcm.layout, pcm.samples.data(), pcm.samples.size(), rate);
	// QMIXPLAYPARAMS +0x18 / +0x1C (0x10012949): only when both are set and start < end; AL wants them inside the wave
	const auto frames = static_cast<int32_t>(pcm.Frames());
	if (sound.loopStart >= 0 && sound.loopEnd > sound.loopStart && sound.loopEnd <= frames)
	{
		device::SetBufferLoopPoints(id, sound.loopStart, sound.loopEnd);
	}
	sound.bufferId = id;
	sound.channelLayout = pcm.layout;
	sound.sizeInBytes = pcm.samples.size() * sizeof(int16_t);
	sound.duration = rate > 0 ? static_cast<float>(pcm.Frames()) / static_cast<float>(rate) : 0.0f;
	g_Buffers.push_back(id);
	++g_Made;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"),
		                   "Wave buffer {} for {} (format {:#x}, {} frames at {} Hz, {:.2f} s, loop {}..{}), {} made", id,
		                   sound.name, sound.waveFormat, frames, rate, sound.duration, sound.loopStart, sound.loopEnd, g_Made);
	}
	return id;
}

void wave_buffers::Release(Sound& sound)
{
	if (sound.bufferId == 0)
	{
		return;
	}
	const auto found = std::find(g_Buffers.begin(), g_Buffers.end(), sound.bufferId);
	if (found != g_Buffers.end())
	{
		if (device::IsOpen())
		{
			device::DeleteBuffer(sound.bufferId);
		}
		g_Buffers.erase(found);
	}
	sound.bufferId = 0;
}

void wave_buffers::DeleteAll()
{
	if (device::IsOpen())
	{
		device::DeleteBuffers(g_Buffers);
	}
	g_Buffers.clear();
}

size_t wave_buffers::Alive()
{
	return g_Buffers.size();
}

size_t wave_buffers::Made()
{
	return g_Made;
}
