/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleChallengePictures.h"

#include <string_view>

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
/// Writes a picture, RGB with its top row first, to its texture, made the first time: as the game keeps it, five bits a
/// colour, the texture the same 16-bit form as the land's own
void Upload(std::optional<graphics::TextureHandle>& texture, std::span<const uint8_t> rgb, uint32_t width, uint32_t height,
            std::string_view name)
{
	const size_t count = static_cast<size_t>(width) * height;
	if (rgb.size() < count * 3)
	{
		return;
	}
	if (!texture.has_value())
	{
		texture =
		    graphics::fromBgfx(bgfx::createTexture2D(static_cast<uint16_t>(width), static_cast<uint16_t>(height), false, 1,
		                                             bgfx::TextureFormat::BGR5A1, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP));
		bgfx::setName(graphics::toBgfx(*texture), name.data(), static_cast<int32_t>(name.size()));
	}
	const auto texels = picture_sizes::Texels(rgb.first(count * 3));
	const auto* memory = bgfx::copy(texels.data(), static_cast<uint32_t>(texels.size() * sizeof(uint16_t)));
	bgfx::updateTexture2D(graphics::toBgfx(*texture), 0, 0, 0, 0, static_cast<uint16_t>(width), static_cast<uint16_t>(height),
	                      memory);
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
	for (auto& place : _places)
	{
		Destroy(place);
	}
	_places.clear();
	_reread = true;
}

void TempleChallengePictures::DropFull(Place& place)
{
	if (place.full.has_value())
	{
		bgfx::destroy(graphics::toBgfx(*place.full));
		place.full.reset();
	}
	place.written.full.reset();
}

void TempleChallengePictures::Destroy(Place& place)
{
	if (place.small.has_value())
	{
		bgfx::destroy(graphics::toBgfx(*place.small));
		place.small.reset();
	}
	place.written.small.reset();
	DropFull(place);
}

void TempleChallengePictures::Update(const ecs::Registry& registry, std::optional<uint32_t> highDetailSlot)
{
	const auto records = ecs::challenge_records::InOrder(registry);
	// Slots past the records' count go, with their textures
	while (_places.size() > records.size())
	{
		Destroy(_places.back());
		_places.pop_back();
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
			Destroy(place);
			place.record = records[i];
		}
		const auto* picture = registry.TryGet<const ecs::components::ChallengePicture>(records[i]);
		if (picture == nullptr)
		{
			continue;
		}
		const auto steps = temple_challenge_pictures::StepsFor(place.written, picture->taken, highDetailSlot == i, _reread);
		if (steps.writeSmall)
		{
			Upload(place.small, picture->small, picture_sizes::k_SmallWidth, picture_sizes::k_SmallHeight,
			       fmt::format("Challenge {} low detail", i));
			place.written.small = picture->taken;
		}
		if (steps.writeFull)
		{
			Upload(place.full, picture->full, picture_sizes::k_Width, picture_sizes::k_Height,
			       fmt::format("Challenge {} high detail", i));
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
		// The picture looked at is drawn in high detail, the others in low detail
		const auto& place = _places[picture.slot];
		const auto& texture = place.full.has_value() ? place.full : place.small;
		if (texture.has_value())
		{
			textures.push_back({.subMesh = picture.subMesh, .texture = *texture});
		}
	}
	return textures;
}
