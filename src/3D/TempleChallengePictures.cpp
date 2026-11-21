/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleChallengePictures.h"

#include <algorithm>
#include <string_view>
#include <vector>

#include <bgfx/bgfx.h>
#include <fmt/format.h>

#include "ECS/ChallengePictures.h"
#include "ECS/ChallengeRecords.h"
#include "ECS/Components/ChallengePicture.h"
#include "ECS/Registry.h"
#include "Graphics/GraphicsHandleBgfx.h"

using namespace openblack;
namespace picture_sizes = openblack::ecs::challenge_pictures;

namespace
{
/// The sheet, made the first time, all of it black: the same 16-bit form as the land's own textures
void MakeSheet(std::optional<graphics::TextureHandle>& sheet, std::string_view name)
{
	if (sheet.has_value())
	{
		return;
	}
	constexpr auto k_Size = static_cast<uint16_t>(temple_challenge_pictures::k_SheetSize);
	const std::vector<uint16_t> black(static_cast<size_t>(k_Size) * k_Size, 0);
	// made with no memory, so the pictures can be written into it; a texture made with its memory is never updated
	sheet = graphics::fromBgfx(bgfx::createTexture2D(k_Size, k_Size, false, 1, bgfx::TextureFormat::BGR5A1,
	                                                 BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, nullptr));
	bgfx::updateTexture2D(graphics::toBgfx(*sheet), 0, 0, 0, 0, k_Size, k_Size,
	                      bgfx::copy(black.data(), static_cast<uint32_t>(black.size() * sizeof(uint16_t))));
	bgfx::setName(graphics::toBgfx(*sheet), name.data(), static_cast<int32_t>(name.size()));
}

/// Writes a picture, RGB with its top row first, into its place on its sheet: as the game keeps it, five bits a colour
void Upload(const graphics::TextureHandle& sheet, std::span<const uint8_t> rgb, uint32_t x, uint32_t y, uint32_t width,
            uint32_t height)
{
	const size_t count = static_cast<size_t>(width) * height;
	if (rgb.size() < count * 3)
	{
		return;
	}
	const auto texels = picture_sizes::Texels(rgb.first(count * 3));
	const auto* memory = bgfx::copy(texels.data(), static_cast<uint32_t>(texels.size() * sizeof(uint16_t)));
	bgfx::updateTexture2D(graphics::toBgfx(sheet), 0, 0, static_cast<uint16_t>(x), static_cast<uint16_t>(y),
	                      static_cast<uint16_t>(width), static_cast<uint16_t>(height), memory);
}
} // namespace

temple_challenge_pictures::Steps temple_challenge_pictures::StepsFor(const Written& written, uint32_t taken, bool highDetail,
                                                                     bool reread)
{
	if (taken == 0)
	{
		return {};
	}
	Steps steps;
	steps.writeSmall = written.small != taken && (reread || !written.small.has_value());
	if (highDetail)
	{
		steps.writeFull = written.full != taken;
	}
	else
	{
		// Only the picture looked at is kept in high detail
		steps.dropFull = written.full.has_value();
	}
	return steps;
}

TempleChallengePictures::~TempleChallengePictures()
{
	Clear();
}

void TempleChallengePictures::Clear()
{
	for (auto& sheet : _lowSheets)
	{
		if (sheet.has_value())
		{
			bgfx::destroy(graphics::toBgfx(*sheet));
			sheet.reset();
		}
	}
	if (_highSheet.has_value())
	{
		bgfx::destroy(graphics::toBgfx(*_highSheet));
		_highSheet.reset();
	}
	_highSheetPair.reset();
	_places.clear();
	_reread = true;
}

void TempleChallengePictures::DropFull(Place& place)
{
	place.written.full.reset();
	// The high detail sheet goes once no slot of its pair is looked at
	const bool kept = std::ranges::any_of(_places, [](const Place& other) { return other.written.full.has_value(); });
	if (!kept && _highSheet.has_value())
	{
		bgfx::destroy(graphics::toBgfx(*_highSheet));
		_highSheet.reset();
		_highSheetPair.reset();
	}
}

void TempleChallengePictures::WriteFull(size_t slot, std::span<const uint8_t> rgb)
{
	const auto place = temple_challenge_pictures::HighDetailPlace(static_cast<uint32_t>(slot));
	// The sheet holds the pair's two pictures: a new pair's sheet starts black
	if (_highSheetPair != place.sheet && _highSheet.has_value())
	{
		bgfx::destroy(graphics::toBgfx(*_highSheet));
		_highSheet.reset();
	}
	MakeSheet(_highSheet, fmt::format("Challenges {} and {} high detail", place.sheet * 2, place.sheet * 2 + 1));
	_highSheetPair = place.sheet;
	Upload(*_highSheet, rgb, place.x, place.y, picture_sizes::k_Width, picture_sizes::k_Height);
}

void TempleChallengePictures::Update(const ecs::Registry& registry, std::optional<uint32_t> highDetailSlot)
{
	const auto records = ecs::challenge_records::InOrder(registry);
	// Slots past the records' count go, with their textures
	if (_places.size() > records.size())
	{
		_places.resize(records.size());
	}
	for (size_t i = 0; i < records.size(); ++i)
	{
		if (i == _places.size())
		{
			_places.push_back({.record = records[i]});
		}
		auto& place = _places[i];
		if (place.record != records[i])
		{
			place = {.record = records[i]};
		}
		const auto* picture = registry.TryGet<const ecs::components::ChallengePicture>(records[i]);
		if (picture == nullptr)
		{
			continue;
		}
		const auto steps = temple_challenge_pictures::StepsFor(place.written, picture->taken, highDetailSlot == i, _reread);
		const auto low = temple_challenge_pictures::LowDetailPlace(static_cast<uint32_t>(i));
		if (steps.writeSmall && low.sheet < _lowSheets.size())
		{
			MakeSheet(_lowSheets.at(low.sheet),
			          fmt::format("Challenges {} to {} low detail", low.sheet * 8, low.sheet * 8 + 7));
			Upload(*_lowSheets.at(low.sheet), picture->small, low.x, low.y, picture_sizes::k_SmallWidth,
			       picture_sizes::k_SmallHeight);
			place.written.small = picture->taken;
		}
		if (steps.writeFull)
		{
			WriteFull(i, picture->full);
			// the sheet's other half holds the pair's other picture, as the game's file of the pair keeps it
			const size_t mate = i ^ 1u;
			if (mate < records.size())
			{
				if (const auto* other = registry.TryGet<const ecs::components::ChallengePicture>(records[mate]);
				    other != nullptr && other->taken != 0)
				{
					WriteFull(mate, other->full);
				}
			}
			place.written.full = picture->taken;
		}
		if (steps.dropFull)
		{
			DropFull(place);
		}
	}
	_reread = false;
}

std::vector<TempleSubMeshTexture>
TempleChallengePictures::GetTextures(std::span<const TempleChallengeControls::Picture> pictures) const
{
	std::vector<TempleSubMeshTexture> textures;
	for (const auto& picture : pictures)
	{
		if (picture.slot >= _places.size())
		{
			continue;
		}
		// The picture looked at is drawn from the high detail sheet, the others from their low detail sheets, each
		// through its rectangle of its sheet
		const auto& place = _places[picture.slot];
		const auto slot = static_cast<uint32_t>(picture.slot);
		if (place.written.full.has_value() && _highSheet.has_value())
		{
			textures.push_back({.subMesh = picture.subMesh,
			                    .texture = *_highSheet,
			                    .uv = temple_challenge_pictures::HighDetailPlace(slot).uv});
			continue;
		}
		const auto low = temple_challenge_pictures::LowDetailPlace(slot);
		if (place.written.small.has_value() && low.sheet < _lowSheets.size() && _lowSheets.at(low.sheet).has_value())
		{
			textures.push_back({.subMesh = picture.subMesh, .texture = *_lowSheets.at(low.sheet), .uv = low.uv});
		}
	}
	return textures;
}
