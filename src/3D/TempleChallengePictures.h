/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec4.hpp>

#include "3D/TempleChallengeControls.h"
#include "3D/TempleInteriorInterface.h"
#include "Graphics/GraphicsHandle.h"

namespace openblack
{
namespace ecs
{
class Registry;
}

/// What a slot's picture textures are to do this frame, from the count of pictures its record has taken: write the low
/// detail one again once the record has a new picture, write the high detail one again for the slot looked at, and drop
/// the high detail one of any other slot
namespace temple_challenge_pictures
{

/// The record's count of pictures taken when each texture was last written; none for a texture not made
struct Written
{
	std::optional<uint32_t> small;
	std::optional<uint32_t> full;
};

struct Steps
{
	bool writeSmall {false};
	bool writeFull {false};
	bool dropFull {false};

	bool operator==(const Steps&) const = default;
};

/// `taken` 0: the record has no picture yet, so nothing is written. `reread`: the player has just come into the room,
/// when the game reads the low detail pictures again; until then a slot already shown keeps its low detail picture.
[[nodiscard]] Steps StepsFor(const Written& written, uint32_t taken, bool highDetail, bool reread = true);

/// The pictures share sheets of 256 by 256: eight low detail pictures a sheet, two high detail ones. Where a slot's
/// picture is on its sheet, and the rectangle of the sheet its picture submesh is drawn with, from its low corner to
/// its high one
struct SheetPlace
{
	uint32_t sheet;
	uint32_t x;
	uint32_t y;
	glm::vec4 uv;
};
inline constexpr uint32_t k_SheetSize = 256;
/// The low detail sheets the room has, for its 21 pictures
inline constexpr uint32_t k_LowDetailSheets = 3;
[[nodiscard]] constexpr SheetPlace LowDetailPlace(uint32_t slot) noexcept
{
	const float u = static_cast<float>(slot & 1u) * 0.5f;
	const float v = static_cast<float>((slot & 7u) >> 1u) * 0.25f;
	return {.sheet = slot >> 3u,
	        .x = (slot & 1u) * 128u,
	        .y = ((slot & 7u) >> 1u) * 64u,
	        .uv = glm::vec4(u, v, u + 0.5f, v + 0.25f)};
}
[[nodiscard]] constexpr SheetPlace HighDetailPlace(uint32_t slot) noexcept
{
	const float v = static_cast<float>(slot & 1u) * 0.5f;
	return {.sheet = slot >> 1u, .x = 0u, .y = (slot & 1u) * 128u, .uv = glm::vec4(0.0f, v, 1.0f, v + 0.5f)};
}

} // namespace temple_challenge_pictures

/// The challenge room's pictures on its walls: each started slot shows its record's picture in low detail, and the slot
/// last looked at shows it in high detail. The textures follow the records' pictures as they are taken, and go as the
/// player leaves the temple.
class TempleChallengePictures
{
public:
	TempleChallengePictures() = default;
	~TempleChallengePictures();
	TempleChallengePictures(const TempleChallengePictures&) = delete;
	TempleChallengePictures& operator=(const TempleChallengePictures&) = delete;
	TempleChallengePictures(TempleChallengePictures&&) = delete;
	TempleChallengePictures& operator=(TempleChallengePictures&&) = delete;

	/// Brings the textures up to date with the player's challenge records, in the order they started, and the slot
	/// looked at in high detail
	void Update(const ecs::Registry& registry, std::optional<uint32_t> highDetailSlot);
	/// The pictures' textures on the room's picture submeshes, for the slots that have one
	[[nodiscard]] std::vector<TempleSubMeshTexture>
	GetTextures(std::span<const TempleChallengeControls::Picture> pictures) const;
	/// Every texture goes
	void Clear();
	/// The player comes into the room: the low detail pictures are read again at the next update
	void Reread() { _reread = true; }

private:
	struct Place
	{
		entt::entity record {entt::null};
		temple_challenge_pictures::Written written;
	};
	void DropFull(Place& place);
	void WriteFull(size_t slot, std::span<const uint8_t> rgb);

	std::vector<Place> _places;
	std::array<std::optional<graphics::TextureHandle>, temple_challenge_pictures::k_LowDetailSheets> _lowSheets;
	/// The high detail sheet of the pair of slots of the one looked at
	std::optional<graphics::TextureHandle> _highSheet;
	std::optional<uint32_t> _highSheetPair;
	bool _reread {true};
};

} // namespace openblack
