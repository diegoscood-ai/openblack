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

#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>

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
		std::optional<graphics::TextureHandle> small;
		std::optional<graphics::TextureHandle> full;
		temple_challenge_pictures::Written written;
	};
	static void Destroy(Place& place);
	static void DropFull(Place& place);

	std::vector<Place> _places;
	bool _reread {true};
};

} // namespace openblack
