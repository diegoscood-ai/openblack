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
#include <vector>

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/Reward.h"
#include "ECS/RewardRules.h"
#include "Enums.h"

namespace openblack::particles::draw
{
struct Frame;
}

namespace openblack::ecs::systems
{

/// A reward chest as a saved game holds it, for its load to make it again
struct SavedRewardChest
{
	/// Its place, in the world; one falling is drawn above it by its clock
	glm::vec3 position {0.0f};
	/// Its y angle, in radians, and its size
	float heading {0.0f};
	float scale {1.0f};
	RewardObjectInfo type {RewardObjectInfo::None};
	std::optional<PlayerNames> player;
	/// The town it was given to, none for none
	entt::entity town {entt::null};
	components::Reward::State state {components::Reward::State::Landed};
	/// Its player has tapped it open
	bool opened {false};
	/// Its own clock, in seconds of the game's time
	float seconds {0.0f};
	/// It has landed, and goes into the map's cells at the next game turn
	bool goesIntoMap {false};
	/// How long its dust has left to show, in whole milliseconds
	uint32_t dustMilliseconds {0};
	/// It has landed (or was put on the land): it is then put into the map's cells at once
	bool landed {false};
	/// The miracle its town learnt from it, spinning above it while it is open; none for none
	entt::entity townSeed {entt::null};
};

/// The reward chests the scripts give: put on the land, or dropped from the sky with a thump that shakes the camera and
/// a cloud of dust
class RewardSystemInterface
{
public:
	virtual ~RewardSystemInterface() = default;

	/// A reward chest of a kind for a player (and a town, or none) at a point: on the land, or falling from the sky
	virtual entt::entity Create(glm::vec3 position, RewardObjectInfo type, std::optional<PlayerNames> player, entt::entity town,
	                            bool fromSky) = 0;
	/// A chest a saved game's load makes again, as the game's load makes it: with its saved state, clock and dust time,
	/// its dust sprites made again as a new chest's are (the same random draws), the opened model when it is open, and
	/// once landed in the map's cells at once (not at the next turn). It has no turns left once open (the turns are not
	/// saved, and a loaded chest starts with none), so an opened one goes at the next game turn. It is newer than the
	/// chests made before it.
	virtual entt::entity Restore(const SavedRewardChest& saved) = 0;
	/// A player's hand taps a chest: only its own player's counts, and only until it is open. The tap is heard by the
	/// player at this computer; a chest on the land opens, giving what it holds. Whether the tap counted.
	virtual bool Tap(entt::entity chest, PlayerNames player) = 0;
	/// Whether a chest has been tapped open
	[[nodiscard]] virtual bool IsOpened(entt::entity chest) const = 0;
	/// Every frame, by the game's time: the chests from the sky fall and turn, and thump down; their dust fades; an
	/// opened chest's lid lifts
	virtual void Update(float milliseconds) = 0;
	/// Every game turn: a chest that has just landed goes into the map's cells, and an opened chest goes once its time is
	/// up
	virtual void ProcessTurn() = 0;
	/// The chests' dust drawn this frame, added to the particles' frame
	virtual void CollectDrawFrame(particles::draw::Frame& frame) const = 0;
	/// The chests' dust sprites this frame, made by Update for the renderer's sprite list to read (ours: our renderer
	/// draws them with the other smoke sprites)
	[[nodiscard]] virtual const std::vector<reward::DustDraw>& GetDustSprites() const = 0;
};

} // namespace openblack::ecs::systems
