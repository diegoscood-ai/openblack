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

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureCatch.h"
#include "ECS/Effects/EffectValues.h"
#include "Enums.h"

namespace openblack::magic
{
struct EffectSource;
}

namespace openblack::ecs
{
class Registry;
struct PhysicsEntry;
namespace systems
{
class BuildingDamageSystemInterface;
class HandSystemInterface;
} // namespace systems
} // namespace openblack::ecs

/// What the game's kinds of thing need of the rest of the game as they fly, are struck and come down: the entities, the
/// hand system, whose part the trees' and pots' physics are, the broken buildings, whose part the fragments' physics
/// is, the player at this machine, the effects, the creatures' minds and their bodies' sway. The game's own world
/// works through the game's services; tests give a fake.
namespace openblack::ecs::physics_hooks
{

class World
{
public:
	virtual ~World() = default;

	[[nodiscard]] virtual Registry& Entities() = 0;
	/// The hand system, which plays the trees' and pots' part
	[[nodiscard]] virtual systems::HandSystemInterface& HandPart() = 0;
	/// The broken buildings, which play the fragments' part
	[[nodiscard]] virtual systems::BuildingDamageSystemInterface& BuildingDamage() = 0;

	/// The player at this machine, whose hand throws
	[[nodiscard]] virtual PlayerNames LocalPlayer() const = 0;
	/// Whether a thing is a toy
	[[nodiscard]] virtual bool IsToy(entt::entity object) const = 0;

	/// The game's crush, none before the game's data is loaded
	[[nodiscard]] virtual std::optional<effects::EffectValues> Crush() const = 0;
	virtual void ApplyEffect(entt::entity object, const effects::EffectValues& values, const magic::EffectSource& source) = 0;

	/// Feedback that only moves how a creature feels about its player, from -1 to 1
	virtual void UpdateAttitudeFromFeedback(entt::entity creature, float feedback) = 0;
	/// Every source of a type in a creature's desires goes up or down by an amount
	virtual void ChangeDesireSource(entt::entity creature, uint32_t source, float amount) = 0;
	/// A force kicks a creature's body at a point and sets it swaying, unless what the body is doing takes no kick
	virtual void KickSway(entt::entity creature, glm::vec3 force, glm::vec3 point) = 0;

	/// What a temple's heart struck in the physics passes the blow on to, beaming at it while the thing breaks buildings:
	/// a building or a homeless villager of its player's towns, else the heart itself. Without temples, the heart
	[[nodiscard]] virtual entt::entity HeartPassesBlowTo(entt::entity heart, bool /*breaksBuildings*/) { return heart; }
	/// The heart of the temple a part of it (a worship site, its altar) belongs to, which takes the blows the part is
	/// struck by; none for anything else, or for a temple with no heart
	[[nodiscard]] virtual entt::entity CitadelPartHeart(entt::entity /*part*/) { return entt::null; }
	/// A building takes a blow passed on to it as if the thing in the physics entry had struck it. False when the entry's
	/// object went away
	virtual bool StrikeBuilding(entt::entity /*building*/, PhysicsEntry& /*entry*/) { return true; }
	/// The creatures a thing starting to fly is offered to, in the order they are asked
	[[nodiscard]] virtual std::vector<entt::entity> Creatures() { return {}; }
	/// What decides whether a creature may set about catching anything at all
	[[nodiscard]] virtual creature_catch::Eligibility CatchEligibility(entt::entity /*creature*/)
	{
		return {.heldByHand = true};
	}
	/// How heavy a creature is, and a thing
	[[nodiscard]] virtual float CreatureWeight(entt::entity /*creature*/) { return 0.0f; }
	[[nodiscard]] virtual float ThingWeight(entt::entity /*object*/) { return 0.0f; }
	/// Whether the thing's own state lets it be caught: its kind can be picked up, and it says it may
	[[nodiscard]] virtual bool MayBeCaught(entt::entity /*object*/) { return false; }
	/// The creature's player
	[[nodiscard]] virtual PlayerNames OwnerOf(entt::entity /*creature*/) { return PlayerNames::NEUTRAL; }
	/// Whether the player is allied to the other (ecs::alliances); nobody is without alliances
	[[nodiscard]] virtual bool IsAllied(PlayerNames /*player*/, PlayerNames /*other*/) { return false; }
	/// The game's own draw, below a number
	[[nodiscard]] virtual uint32_t GameRand(uint32_t /*range*/) { return 0; }
	/// What a creature's reach to catch depends on besides the thing: where it stands, its size, the scale its model is
	/// drawn at, when its catching hand closes and its step towards a catch. None without its catch moment or its step
	[[nodiscard]] virtual std::optional<creature_catch::Approach> CatchApproach(entt::entity /*creature*/)
	{
		return std::nullopt;
	}
	/// The creature stops where it is and is made to set about catching the thing, as a plan of play
	virtual void SetAboutCatching(entt::entity /*creature*/, entt::entity /*object*/) {}
};

} // namespace openblack::ecs::physics_hooks
