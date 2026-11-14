/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleSaveGamePictures.h"

#include <algorithm>
#include <string_view>

#include <SavePictureMaker.h>
#include <SavePictures.h>
#include <bgfx/bgfx.h>
#include <fmt/format.h>

#include "ECS/ChallengePictures.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Temple/SaveGameRoomRules.h"

using namespace openblack;
namespace room = openblack::temple::save_game_room;

namespace
{
constexpr auto k_Side = static_cast<uint16_t>(gamesave::k_PictureSide);
constexpr uint64_t k_Clamp = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;

void Destroy(std::optional<graphics::TextureHandle>& texture)
{
	if (texture.has_value())
	{
		bgfx::destroy(graphics::toBgfx(*texture));
		texture.reset();
	}
}

/// A texture of RGB texels with their top row first, in the game's 16-bit form, five bits a colour
graphics::TextureHandle SixteenBitTexture(std::span<const uint8_t> rgb, std::string_view name)
{
	const auto texels = ecs::challenge_pictures::Texels(rgb);
	const auto texture = graphics::fromBgfx(
	    bgfx::createTexture2D(k_Side, k_Side, false, 1, bgfx::TextureFormat::BGR5A1, k_Clamp,
	                          bgfx::copy(texels.data(), static_cast<uint32_t>(texels.size() * sizeof(uint16_t)))));
	bgfx::setName(graphics::toBgfx(texture), name.data(), static_cast<int32_t>(name.size()));
	return texture;
}

/// A texture of a file of pictures, as the file keeps it: compressed, in blocks of 4 by 4 texels. Where the renderer
/// cannot sample such blocks, the texels they give
std::optional<graphics::TextureHandle> PictureTexture(const std::optional<std::vector<uint8_t>>& bytes, std::string_view name)
{
	const auto file = bytes.has_value() ? gamesave::ReadPictureFile(*bytes) : std::nullopt;
	if (!file.has_value() || file->blocks.size() < gamesave::k_PictureDataSize)
	{
		return std::nullopt;
	}
	const auto* caps = bgfx::getCaps();
	if (caps == nullptr || (caps->formats[bgfx::TextureFormat::BC1] & BGFX_CAPS_FORMAT_TEXTURE_2D) == 0)
	{
		return SixteenBitTexture(gamesave::DecodePicture(file->blocks), name);
	}
	const auto texture = graphics::fromBgfx(
	    bgfx::createTexture2D(k_Side, k_Side, false, 1, bgfx::TextureFormat::BC1, k_Clamp,
	                          bgfx::copy(file->blocks.data(), static_cast<uint32_t>(gamesave::k_PictureDataSize))));
	bgfx::setName(graphics::toBgfx(texture), name.data(), static_cast<int32_t>(name.size()));
	return texture;
}
} // namespace

TempleSaveGamePictures::~TempleSaveGamePictures()
{
	Clear();
	Destroy(_empty);
}

void TempleSaveGamePictures::SetEmptyPicture(std::span<const uint8_t> rgb)
{
	Destroy(_empty);
	if (rgb.size() != gamesave::k_PictureTexelsSize)
	{
		return;
	}
	_empty = SixteenBitTexture(rgb, "Saved game, empty picture");
}

void TempleSaveGamePictures::Clear()
{
	for (auto& texture : _small)
	{
		Destroy(texture);
	}
	Destroy(_large);
	_highDetail.reset();
}

void TempleSaveGamePictures::ReadSmall(const ReadFile& read)
{
	for (uint32_t file = 0; file < k_SmallFiles; ++file)
	{
		Destroy(_small.at(file));
		const auto first = file * 8;
		_small.at(file) =
		    PictureTexture(read ? read(gamesave::SmallPictureFileName(first)) : std::nullopt,
		                   fmt::format("Saved games {} to {} low detail", first, std::min(first + 7, room::k_Slots - 1)));
	}
}

void TempleSaveGamePictures::ReadLarge(uint32_t slot, const ReadFile& read)
{
	Destroy(_large);
	const auto first = slot & ~1u;
	_large = PictureTexture(read ? read(gamesave::LargePictureFileName(slot)) : std::nullopt,
	                        fmt::format("Saved games {} and {} high detail", first, first + 1));
}

void TempleSaveGamePictures::Enter(const ReadFile& read)
{
	ReadSmall(read);
	Destroy(_large);
	_highDetail.reset();
}

void TempleSaveGamePictures::LookAt(uint32_t slot, const ReadFile& read)
{
	ReadLarge(slot, read);
	_highDetail = slot;
}

void TempleSaveGamePictures::AfterSave(std::optional<uint32_t> slot, const ReadFile& read)
{
	const auto saved = slot.has_value() ? slot : _highDetail;
	if (!saved.has_value())
	{
		ReadSmall(read);
		return;
	}
	const auto pictures = room::PicturesAfterSave(*saved, _highDetail);
	if (pictures.smallPictures)
	{
		ReadSmall(read);
	}
	if (pictures.largePicture)
	{
		ReadLarge(*saved, read);
	}
}

std::vector<TempleSubMeshTexture> TempleSaveGamePictures::GetTextures(std::span<const TempleSaveGameControls::Picture> pictures,
                                                                      const TempleSaveGameControls& controls, bool inTheRoom,
                                                                      float alignment) const
{
	std::vector<TempleSubMeshTexture> textures;
	for (const auto& picture : pictures)
	{
		const auto shown = room::PictureOf(picture.slot, controls.HasSave(picture.slot), inTheRoom, _highDetail, alignment);
		std::optional<graphics::TextureHandle> texture;
		switch (shown.sheet)
		{
		case room::PictureSheet::Empty:
			texture = _empty;
			break;
		case room::PictureSheet::Small:
			texture = shown.file < _small.size() ? _small.at(shown.file) : std::nullopt;
			break;
		case room::PictureSheet::Large:
			texture = _large;
			break;
		}
		// A picture whose file could not be read keeps the mesh's own texture
		if (texture.has_value())
		{
			textures.push_back({.subMesh = picture.subMesh,
			                    .texture = *texture,
			                    .uv = glm::vec4(shown.uv.u0, shown.uv.v0, shown.uv.u1, shown.uv.v1)});
		}
	}
	return textures;
}
