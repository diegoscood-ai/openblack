/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DanceFile.h"

#include <cstring>

#include <algorithm>
#include <bit>
#include <utility>

using namespace openblack::dance;

namespace
{
/// The smallest a key frame can be: its time, its flags and its action count
constexpr std::size_t k_SmallestKeyFrame = 12;

class Reader
{
public:
	explicit Reader(std::span<const uint8_t> buffer)
	    : _buffer(buffer)
	{
	}

	bool U32(uint32_t& value)
	{
		if (_buffer.size() - _offset < sizeof(value))
		{
			return false;
		}
		std::memcpy(&value, _buffer.data() + _offset, sizeof(value));
		_offset += sizeof(value);
		return true;
	}

	bool F32(float& value)
	{
		uint32_t bits = 0;
		if (!U32(bits))
		{
			return false;
		}
		value = std::bit_cast<float>(bits);
		return true;
	}

	bool Bytes(std::string& text, uint32_t count)
	{
		if (_buffer.size() - _offset < count)
		{
			return false;
		}
		text.assign(reinterpret_cast<const char*>(_buffer.data() + _offset), count);
		_offset += count;
		return true;
	}

	[[nodiscard]] std::size_t Left() const { return _buffer.size() - _offset; }

private:
	std::span<const uint8_t> _buffer;
	std::size_t _offset {0};
};

bool ReadAction(Reader& reader, DanceAction& action)
{
	uint32_t groups = 0;
	if (!reader.U32(groups))
	{
		return false;
	}
	for (uint32_t i = 0; i < groups; ++i)
	{
		uint32_t group = 0;
		if (!reader.U32(group))
		{
			return false;
		}
		action.groups.push_back(group);
	}
	if (!reader.U32(action.type))
	{
		return false;
	}
	// The argument is read a byte at a time into its 56 bytes
	for (auto& argument : action.arguments)
	{
		if (!reader.U32(argument))
		{
			return false;
		}
	}
	return true;
}

bool ReadKeyFrame(Reader& reader, DanceKeyFrame& keyFrame)
{
	uint32_t actions = 0;
	if (!reader.F32(keyFrame.time) || !reader.U32(keyFrame.flags) || !reader.U32(actions))
	{
		return false;
	}
	// Each action goes after the others, whatever groups it shares with them
	for (uint32_t i = 0; i < actions; ++i)
	{
		DanceAction action;
		if (!ReadAction(reader, action))
		{
			return false;
		}
		keyFrame.actions.push_back(std::move(action));
	}
	return true;
}

/// A key frame goes before the first one whose time is not less than its own (a time that is not a number, either
/// way, goes before the first)
void Insert(std::vector<DanceKeyFrame>& keyFrames, DanceKeyFrame keyFrame)
{
	const auto at =
	    std::ranges::find_if(keyFrames, [&keyFrame](const DanceKeyFrame& there) { return !(keyFrame.time > there.time); });
	keyFrames.insert(at, std::move(keyFrame));
}
} // namespace

std::string_view openblack::dance::ResultToStr(DanceResult result)
{
	switch (result)
	{
	case DanceResult::Success:
		return "Success";
	case DanceResult::ErrTruncated:
		return "The dance file ends too soon";
	case DanceResult::ErrTrailingBytes:
		return "The dance file has bytes after its end";
	}
	return "Unknown";
}

float DanceAction::Float(std::size_t index) const
{
	return std::bit_cast<float>(arguments.at(index));
}

DanceResult DanceFile::Open(std::span<const uint8_t> buffer)
{
	// The key frames it had go first
	keyFrames.clear();
	groupNames.clear();
	Reader reader(buffer);
	uint32_t keyFrameCount = 0;
	if (!reader.U32(version) || !reader.U32(keyFrameCount))
	{
		return DanceResult::ErrTruncated;
	}
	keyFrames.reserve(std::min<std::size_t>(keyFrameCount, reader.Left() / k_SmallestKeyFrame));
	for (uint32_t i = 0; i < keyFrameCount; ++i)
	{
		DanceKeyFrame keyFrame;
		if (!ReadKeyFrame(reader, keyFrame))
		{
			return DanceResult::ErrTruncated;
		}
		Insert(keyFrames, std::move(keyFrame));
	}
	if (!reader.F32(beat) || !reader.U32(groupCount) || !reader.U32(unknown44) || !reader.U32(unknown48) || !reader.U32(loops))
	{
		return DanceResult::ErrTruncated;
	}
	if (version >= 2)
	{
		float facing = 0.0f;
		if (!reader.F32(facing))
		{
			return DanceResult::ErrTruncated;
		}
		angle = facing;
	}
	if (version >= 1)
	{
		for (uint32_t i = 0; i < groupCount; ++i)
		{
			uint32_t length = 0;
			std::string name;
			if (!reader.U32(length) || !reader.Bytes(name, length))
			{
				return DanceResult::ErrTruncated;
			}
			// Kept with a null character after it, so a name ends at its first one
			name.resize(std::min(name.size(), name.find('\0')));
			groupNames.push_back(std::move(name));
		}
	}
	return reader.Left() == 0 ? DanceResult::Success : DanceResult::ErrTrailingBytes;
}
