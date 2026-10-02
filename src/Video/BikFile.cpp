/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BikFile.h"

#include <fstream>
#include <utility>

#include <fmt/format.h>

using namespace openblack::video;

namespace
{
uint32_t ReadU32(const std::vector<uint8_t>& data, size_t offset)
{
	return static_cast<uint32_t>(data[offset]) | static_cast<uint32_t>(data[offset + 1]) << 8 |
	       static_cast<uint32_t>(data[offset + 2]) << 16 | static_cast<uint32_t>(data[offset + 3]) << 24;
}

uint16_t ReadU16(const std::vector<uint8_t>& data, size_t offset)
{
	return static_cast<uint16_t>(data[offset] | data[offset + 1] << 8);
}
} // namespace

bool BikFile::Open(const std::filesystem::path& path)
{
	Close();
	std::ifstream stream(path, std::ios::binary | std::ios::ate);
	if (!stream)
	{
		return Fail(fmt::format("cannot open {}", path.string()));
	}
	const auto size = static_cast<std::streamoff>(stream.tellg());
	if (size < 0)
	{
		return Fail(fmt::format("cannot read {}", path.string()));
	}
	std::vector<uint8_t> data(static_cast<size_t>(size));
	stream.seekg(0);
	if (size > 0 && !stream.read(reinterpret_cast<char*>(data.data()), size))
	{
		return Fail(fmt::format("cannot read {}", path.string()));
	}
	return Parse(std::move(data));
}

bool BikFile::Parse(std::vector<uint8_t> data)
{
	Close();
	if (data.size() < k_HeaderSize)
	{
		return Fail(fmt::format("{} bytes: shorter than the {}-byte header", data.size(), k_HeaderSize));
	}
	if (data[0] != 'B' || data[1] != 'I' || data[2] != 'K')
	{
		return Fail("not a Bink 1 file (no \"BIK\" signature)");
	}
	_revision = static_cast<char>(data[3]);
	_headerFileSize = ReadU32(data, 4);
	_frameCount = ReadU32(data, 8);
	_largestFrameSize = ReadU32(data, 12);
	_width = ReadU32(data, 20);
	_height = ReadU32(data, 24);
	_fpsNumerator = ReadU32(data, 28);
	_fpsDenominator = ReadU32(data, 32);
	_videoFlags = ReadU32(data, 36);
	const uint32_t tracks = ReadU32(data, 40);
	if (_frameCount == 0)
	{
		return Fail("no frames");
	}
	if (_width == 0 || _height == 0)
	{
		return Fail(fmt::format("no picture ({} x {})", _width, _height));
	}
	if (_fpsNumerator == 0 || _fpsDenominator == 0)
	{
		return Fail(fmt::format("no frame rate ({} / {})", _fpsNumerator, _fpsDenominator));
	}

	// the audio track table and the frame table must be inside the file (64-bit sums: the counts are untrusted)
	const uint64_t trackTable = k_HeaderSize;
	const uint64_t frameTable = trackTable + static_cast<uint64_t>(tracks) * k_AudioTrackSize;
	const uint64_t tableEnd = frameTable + (static_cast<uint64_t>(_frameCount) + 1) * 4;
	if (tableEnd > data.size())
	{
		return Fail(fmt::format("the tables of {} tracks and {} frames end at {}, past the {} bytes of the file", tracks,
		                        _frameCount, tableEnd, data.size()));
	}
	_audioTracks.resize(tracks);
	for (uint32_t i = 0; i < tracks; ++i)
	{
		auto& track = _audioTracks[i];
		track.maxBuffer = ReadU32(data, static_cast<size_t>(trackTable) + 4 * i);
		track.sampleRate = ReadU16(data, static_cast<size_t>(trackTable) + 4 * (tracks + i));
		track.flags = ReadU16(data, static_cast<size_t>(trackTable) + 4 * (tracks + i) + 2);
		track.id = ReadU32(data, static_cast<size_t>(trackTable) + 4 * (2 * tracks + i));
	}

	// bik_frames.py: strictly increasing from the end of the table, the last one at the file size
	_offsets.resize(static_cast<size_t>(_frameCount) + 1);
	for (uint32_t i = 0; i <= _frameCount; ++i)
	{
		_offsets[i] = ReadU32(data, static_cast<size_t>(frameTable) + 4 * static_cast<size_t>(i));
	}
	if (Offset(0) < tableEnd)
	{
		return Fail(fmt::format("frame 0 at {}, inside the tables (they end at {})", Offset(0), tableEnd));
	}
	for (uint32_t i = 0; i < _frameCount; ++i)
	{
		if (Offset(i) >= Offset(i + 1))
		{
			return Fail(fmt::format("frame {} at {} is not before frame {} at {}", i, Offset(i), i + 1, Offset(i + 1)));
		}
	}
	if (Offset(_frameCount) != data.size())
	{
		return Fail(fmt::format("the frame table ends at {}, the file has {} bytes", Offset(_frameCount), data.size()));
	}

	// each packet: per audio track a 32-bit size and the bytes, then the video
	_videoStart.resize(_frameCount);
	for (uint32_t i = 0; i < _frameCount; ++i)
	{
		uint64_t at = Offset(i);
		const uint64_t end = Offset(i + 1);
		for (uint32_t t = 0; t < tracks; ++t)
		{
			if (at + 4 > end)
			{
				return Fail(fmt::format("frame {}: no room for the size of audio track {}", i, t));
			}
			const uint32_t size = ReadU32(data, static_cast<size_t>(at));
			if (at + 4 + size > end)
			{
				return Fail(fmt::format("frame {}: audio track {} has {} bytes, past the packet", i, t, size));
			}
			at += 4 + static_cast<uint64_t>(size);
		}
		_videoStart[i] = static_cast<uint32_t>(at);
	}

	_data = std::move(data);
	return true;
}

void BikFile::Close()
{
	_data.clear();
	_data.shrink_to_fit();
	_offsets.clear();
	_videoStart.clear();
	_audioTracks.clear();
	_error.clear();
	_revision = 0;
	_headerFileSize = 0;
	_frameCount = 0;
	_largestFrameSize = 0;
	_width = 0;
	_height = 0;
	_fpsNumerator = 0;
	_fpsDenominator = 1;
	_videoFlags = 0;
}

bool BikFile::Fail(std::string error)
{
	Close();
	_error = std::move(error);
	return false;
}

bool BikFile::IsKeyFrame(uint32_t frame) const
{
	return frame < _frameCount && (_offsets[frame] & k_KeyFrameFlag) != 0;
}

uint32_t BikFile::KeyFrameCount() const
{
	uint32_t count = 0;
	for (uint32_t i = 0; i < _frameCount; ++i)
	{
		count += IsKeyFrame(i) ? 1 : 0;
	}
	return count;
}

std::span<const uint8_t> BikFile::FrameData(uint32_t frame) const
{
	if (frame >= _frameCount)
	{
		return {};
	}
	return std::span<const uint8_t>(_data).subspan(Offset(frame), Offset(frame + 1) - Offset(frame));
}

std::span<const uint8_t> BikFile::VideoData(uint32_t frame) const
{
	if (frame >= _frameCount)
	{
		return {};
	}
	return std::span<const uint8_t>(_data).subspan(_videoStart[frame], Offset(frame + 1) - _videoStart[frame]);
}

std::span<const uint8_t> BikFile::AudioData(uint32_t frame, uint32_t track) const
{
	if (frame >= _frameCount || track >= _audioTracks.size())
	{
		return {};
	}
	size_t at = Offset(frame);
	for (uint32_t t = 0; t < track; ++t)
	{
		at += 4 + ReadU32(_data, at);
	}
	return std::span<const uint8_t>(_data).subspan(at + 4, ReadU32(_data, at));
}
