/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VanillaSaveFile.h"

#include "ObjectStream.h"

using namespace openblack::gamesave;

namespace
{
/// The longest text that a read of the start of the file accepts
constexpr uint32_t k_MostText = 260;

bool ReadText(StreamReader& reader, std::string& text)
{
	uint32_t length = 0;
	if (!reader.Get(length) || length == 0 || length > k_MostText)
	{
		return false;
	}
	text.clear();
	for (uint32_t i = 0; i < length; ++i)
	{
		char letter = 0;
		reader.Get(letter);
		if (letter != 0)
		{
			text.push_back(letter);
		}
	}
	return !reader.Failed();
}

void WriteText(StreamWriter& writer, std::string_view text)
{
	writer.Put(static_cast<uint32_t>(text.size() + 1));
	for (const char letter : text)
	{
		writer.Put(letter);
	}
	writer.Put('\0');
}
} // namespace

std::optional<VanillaFraming> openblack::gamesave::ReadVanillaFraming(std::span<const uint8_t> bytes)
{
	VanillaFraming framing;
	StreamReader reader(bytes);
	if (!reader.GetUncounted(framing.attempts) || !ReadText(reader, framing.revision) || !ReadText(reader, framing.landscape))
	{
		return std::nullopt;
	}
	framing.body = reader.Position();
	framing.checksum = reader.Checksum();
	return framing;
}

std::vector<uint8_t> openblack::gamesave::WriteVanillaFraming(std::string_view landscape)
{
	StreamWriter writer;
	writer.PutUncounted(0);
	WriteText(writer, k_VanillaRevision);
	WriteText(writer, landscape);
	return writer.Take();
}
