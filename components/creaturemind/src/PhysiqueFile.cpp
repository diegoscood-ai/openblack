/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PhysiqueFile.h"

#include <algorithm>
#include <bit>

using namespace openblack::creaturemind;

namespace
{
class Reader
{
public:
	explicit Reader(std::span<const uint8_t> bytes)
	    : _bytes(bytes)
	{
	}

	[[nodiscard]] std::optional<uint32_t> U32()
	{
		if (_bytes.size() - _at < 4)
		{
			return std::nullopt;
		}
		const uint32_t value = static_cast<uint32_t>(_bytes[_at]) | (static_cast<uint32_t>(_bytes[_at + 1]) << 8) |
		                       (static_cast<uint32_t>(_bytes[_at + 2]) << 16) | (static_cast<uint32_t>(_bytes[_at + 3]) << 24);
		_at += 4;
		return value;
	}

	[[nodiscard]] std::optional<float> F32()
	{
		const auto bits = U32();
		return bits.has_value() ? std::optional<float>(std::bit_cast<float>(*bits)) : std::nullopt;
	}

	[[nodiscard]] bool List(std::vector<uint32_t>& list)
	{
		const auto count = U32();
		if (!count.has_value() || *count > k_PhysiqueListCapacity)
		{
			return false;
		}
		list.clear();
		for (uint32_t i = 0; i < *count; ++i)
		{
			const auto value = U32();
			if (!value.has_value())
			{
				return false;
			}
			list.push_back(*value);
		}
		return true;
	}

	[[nodiscard]] bool AtEnd() const { return _at == _bytes.size(); }

private:
	std::span<const uint8_t> _bytes;
	size_t _at {0};
};

void PutU32(std::vector<uint8_t>& out, uint32_t value)
{
	for (int shift = 0; shift < 32; shift += 8)
	{
		out.push_back(static_cast<uint8_t>((value >> shift) & 0xFFu));
	}
}

void PutList(std::vector<uint8_t>& out, const std::vector<uint32_t>& list)
{
	const auto count = static_cast<uint32_t>(std::min<size_t>(list.size(), k_PhysiqueListCapacity));
	PutU32(out, count);
	for (uint32_t i = 0; i < count; ++i)
	{
		PutU32(out, list[i]);
	}
}
} // namespace

std::optional<PhysiqueFileData> openblack::creaturemind::ReadPhysique(std::span<const uint8_t> bytes)
{
	Reader in(bytes);
	PhysiqueFileData data;
	const auto species = in.U32();
	const auto size = in.F32();
	const auto strength = in.F32();
	const auto fatness = in.F32();
	const auto alignment = in.F32();
	if (!species || !size || !strength || !fatness || !alignment || !in.List(data.listA) || !in.List(data.listB) || !in.AtEnd())
	{
		return std::nullopt;
	}
	data.speciesRow = *species;
	data.size = *size;
	data.strength = *strength;
	data.fatness = *fatness;
	data.alignment = *alignment;
	return data;
}

std::vector<uint8_t> openblack::creaturemind::WritePhysique(const PhysiqueFileData& data)
{
	std::vector<uint8_t> out;
	PutU32(out, data.speciesRow);
	PutU32(out, std::bit_cast<uint32_t>(data.size));
	PutU32(out, std::bit_cast<uint32_t>(data.strength));
	PutU32(out, std::bit_cast<uint32_t>(data.fatness));
	PutU32(out, std::bit_cast<uint32_t>(data.alignment));
	PutList(out, data.listA);
	PutList(out, data.listB);
	return out;
}
