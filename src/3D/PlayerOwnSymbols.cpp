/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerOwnSymbols.h"

#include <algorithm>
#include <string_view>

#include <Bitmap.h>
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::player_own_symbols
{

namespace
{
constexpr size_t k_FileHeader = 14;
constexpr uint32_t k_CoreHeader = 12;
constexpr uint32_t k_InfoHeader = 40;
constexpr uint32_t k_RunLength8 = 1;
constexpr uint32_t k_RunLength4 = 2;

[[nodiscard]] uint32_t Word(std::span<const uint8_t> file, size_t at, size_t count)
{
	uint32_t value = 0;
	for (size_t i = 0; i < count && at + i < file.size(); ++i)
	{
		value |= static_cast<uint32_t>(file[at + i]) << (8 * i);
	}
	return value;
}

/// Whether a run-length image's runs cover every texel, row by row, with no jump over texels and no row ended early
[[nodiscard]] bool RunsAreWhole(std::span<const uint8_t> file, size_t at, uint32_t width, uint32_t height, bool four)
{
	uint32_t x = 0;
	uint32_t row = 0;
	while (at + 2 <= file.size())
	{
		const uint32_t count = file[at];
		const uint32_t value = file[at + 1];
		at += 2;
		if (count > 0)
		{
			x += count;
			if (x > width)
			{
				return false;
			}
			continue;
		}
		if (value == 0)
		{
			if (x != width)
			{
				return false;
			}
			x = 0;
			++row;
			continue;
		}
		if (value == 1)
		{
			return (row == height && x == 0) || (row + 1 == height && x == width);
		}
		if (value == 2)
		{
			return false;
		}
		x += value;
		if (x > width)
		{
			return false;
		}
		const uint32_t length = four ? (value + 1u) / 2u : value;
		at += length + (length % 2u);
	}
	return false;
}

/// Whether Windows' reading of a bitmap's header and texels is established: the two header kinds tried, one plane, the
/// rows from the bottom up (Windows does not load a bitmap stored from the top down), and compressed runs that cover
/// every texel
[[nodiscard]] bool ReadingIsKnown(std::span<const uint8_t> file)
{
	const auto headerSize = Word(file, k_FileHeader, 4);
	if (headerSize == k_CoreHeader)
	{
		return Word(file, k_FileHeader + 8, 2) == 1;
	}
	if (headerSize != k_InfoHeader || Word(file, k_FileHeader + 12, 2) != 1)
	{
		return false;
	}
	const auto width = static_cast<int32_t>(Word(file, k_FileHeader + 4, 4));
	const auto height = static_cast<int32_t>(Word(file, k_FileHeader + 8, 4));
	if (width <= 0 || height <= 0)
	{
		return false;
	}
	const auto compression = Word(file, k_FileHeader + 16, 4);
	if (compression == k_RunLength8 || compression == k_RunLength4)
	{
		return RunsAreWhole(file, Word(file, 10, 4), static_cast<uint32_t>(width), static_cast<uint32_t>(height),
		                    compression == k_RunLength4);
	}
	return true;
}
} // namespace

std::string FileName(uint32_t index)
{
	return "Symbol" + std::to_string(index + 1) + ".bmp";
}

std::expected<Picture, Refusal> PictureOf(std::span<const uint8_t> file)
{
	if (file.size() < k_SmallestFile)
	{
		return std::unexpected(Refusal::TooSmall);
	}
	const auto image = rawimage::DecodeBitmap(file);
	if (!image.has_value())
	{
		return std::unexpected(Refusal::NotABitmap);
	}
	if (!ReadingIsKnown(file))
	{
		return std::unexpected(Refusal::NotDetermined);
	}
	Picture picture {};
	const auto right = std::min<uint32_t>(image->width, k_CellSize);
	const auto bottom = std::min<uint32_t>(image->height, k_CellSize);
	for (uint32_t y = 0; y < bottom; ++y)
	{
		for (uint32_t x = 0; x < right; ++x)
		{
			const auto& colour = image->At(x, y);
			const auto grey = GreyOf(colour[0], colour[1], colour[2]);
			if (!grey.has_value())
			{
				return std::unexpected(Refusal::NotDetermined);
			}
			picture.at((static_cast<size_t>(y) * k_CellSize) + x) = *grey;
		}
	}
	return picture;
}

Pictures ReadPictures(const std::function<std::optional<std::vector<uint8_t>>(const std::filesystem::path&)>& read)
{
	Pictures pictures;
	for (uint32_t index = 0; index < k_Count; ++index)
	{
		const auto file = FileName(index);
		const auto bytes = read(file);
		if (!bytes.has_value())
		{
			continue;
		}
		auto picture = PictureOf(*bytes);
		if (!picture.has_value())
		{
			// a file too small is passed over without a word, as the game does
			if (picture.error() == Refusal::NotDetermined)
			{
				SPDLOG_LOGGER_WARN(spdlog::get("game"),
				                   "Own symbols: {} is not read: its greys as the game reads them are not known", file);
			}
			else if (picture.error() == Refusal::NotABitmap)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Own symbols: {} is not a bitmap", file);
			}
			continue;
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Own symbols: {} takes the place of symbol {}", file, index);
		pictures.at(index) = *picture;
	}
	return pictures;
}

bool Merge(Pictures& kept, const Pictures& read)
{
	bool changed = false;
	for (uint32_t index = 0; index < k_Count; ++index)
	{
		if (read.at(index).has_value() && kept.at(index) != read.at(index))
		{
			kept.at(index) = read.at(index);
			changed = true;
		}
	}
	return changed;
}

void Paste(std::span<uint8_t> sheet, uint8_t channels, uint32_t index, const Picture& picture)
{
	if (sheet.size() < static_cast<size_t>(k_SheetSize) * k_SheetSize * channels || index >= k_Count)
	{
		return;
	}
	const auto left = static_cast<size_t>(index % k_CellsPerRow) * k_CellSize;
	const auto top = static_cast<size_t>(index / k_CellsPerRow) * k_CellSize;
	for (size_t y = 0; y < k_CellSize; ++y)
	{
		for (size_t x = 0; x < k_CellSize; ++x)
		{
			const auto texel = ((top + y) * k_SheetSize) + left + x;
			std::fill_n(sheet.begin() + static_cast<std::ptrdiff_t>(texel * channels), channels,
			            picture.at((y * k_CellSize) + x));
		}
	}
}

entt::id_type BlobId(uint32_t index)
{
	return entt::hashed_string(("player_own_symbols/" + std::to_string(index)).c_str()).value();
}

bool ReadForProfile()
{
	if (!Locator::playerProfileSystem::has_value() || !Locator::resources::has_value())
	{
		return false;
	}
	const auto& profiles = Locator::playerProfileSystem::value();
	if (!profiles.CurrentPlayerFolder().has_value())
	{
		return false;
	}
	auto& blobs = Locator::resources::value().GetBlobs();
	const auto pictures =
	    ReadPictures([&profiles](const std::filesystem::path& file) { return profiles.ReadPlayerFile(file); });
	// the pictures kept, from the byte cache
	Pictures kept;
	for (uint32_t index = 0; index < k_Count; ++index)
	{
		if (const auto id = BlobId(index); blobs.Contains(id) && blobs.Handle(id)->size() == std::tuple_size_v<Picture>)
		{
			Picture picture {};
			std::ranges::copy(*blobs.Handle(id), picture.begin());
			kept.at(index) = picture;
		}
	}
	if (!Merge(kept, pictures))
	{
		return false;
	}
	for (uint32_t index = 0; index < k_Count; ++index)
	{
		const auto id = BlobId(index);
		if (!kept.at(index).has_value())
		{
			continue;
		}
		if (blobs.Contains(id))
		{
			blobs.Erase(id);
		}
		blobs.Load(id, resources::BlobLoader::FromBufferTag {},
		           std::vector<uint8_t>(kept.at(index)->begin(), kept.at(index)->end()));
	}
	return true;
}

std::optional<uint8_t> ChannelsOfSheet(std::string_view lowerCaseStem)
{
	if (lowerCaseStem == "choosesymbol")
	{
		return uint8_t {3};
	}
	if (lowerCaseStem == "choosesymbola")
	{
		return uint8_t {1};
	}
	return std::nullopt;
}

void PasteInto(std::span<uint8_t> sheet, uint8_t channels)
{
	if (!Locator::resources::has_value())
	{
		return;
	}
	auto& blobs = Locator::resources::value().GetBlobs();
	for (uint32_t index = 0; index < k_Count; ++index)
	{
		if (!blobs.Contains(BlobId(index)))
		{
			continue;
		}
		const auto& bytes = *blobs.Handle(BlobId(index));
		if (bytes.size() != std::tuple_size_v<Picture>)
		{
			continue;
		}
		Picture picture {};
		std::ranges::copy(bytes, picture.begin());
		Paste(sheet, channels, index, picture);
	}
}

} // namespace openblack::player_own_symbols
