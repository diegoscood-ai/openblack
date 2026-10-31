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

#include <bitset>
#include <optional>

#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Creature/LeashRope.h"
#include "Creature/LeashRules.h"
#include "Enums.h"

namespace openblack::ecs::components
{

/// A creature's leashes: which it can be put on, the one it wears if any, how it is being led, and the area it is kept
/// within
struct CreatureLeash
{
	/// The leash texture and its alpha, Data/Textures/leash.raw and leasha.raw, which the raw textures load as
	static constexpr entt::id_type k_TextureId = entt::hashed_string("raw/leash");
	static constexpr entt::id_type k_AlphaTextureId = entt::hashed_string("raw/leasha");

	/// The leashes the creature knows, by their place in creature_leash::k_Types. Without the learning leash it can't
	/// be leashed at all.
	std::bitset<creature_leash::k_Types.size()> known;

	/// The leash it wears
	struct Worn
	{
		LeashType type {LeashType::Rope};
		/// The player holding it
		PlayerNames holder {PlayerNames::PLAYER_ONE};
		/// Whether the leash makes the creature do anything, as scripts set
		bool works {true};
		/// What it is tied to, or held in the hand when none
		std::optional<entt::entity> tiedTo;
		/// The game turn it was tied
		uint32_t tiedTurn {0};
		/// The rope from the hand, or what it is tied to, to the creature's collar
		leash_rope::Rope rope;
		/// Whether the rope has been placed from its ends yet
		bool ropeStarted {false};
		/// While the hand pulls the tied rope: the ropes from the hand to the collar and to what the leash is tied to
		struct PullRopes
		{
			leash_rope::Rope toCollar;
			leash_rope::Rope toTied;
		};
		std::optional<PullRopes> pull;
	};
	std::optional<Worn> worn;
	/// Whether a leash put on makes the creature do anything, as scripts set. It belongs to the player's leash, so it is
	/// kept while no leash is on, and a leash put on takes it.
	bool leashWorks {true};
	/// The game turn the player's leash was last tied, 0 if it never was. It belongs to the player's leash, so it is kept
	/// while the leash is untied, off or on again, until it is tied once more
	uint32_t lastTiedTurn {0};
	/// The leash picked to put on next, at the citadel or by the hotkeys
	LeashType selected {LeashType::Rope};
	/// Whether a leash has been picked yet: until then the scripts are told none is
	bool picked {false};

	/// How the creature is being led: not at all, walking where the leash takes it, back within its length, or doing
	/// what the leash sent it to do at a point of the land, which keeps it from walking back until it is over
	enum class Control : uint8_t
	{
		Idle,
		Led,
		Sent,
	};
	Control control {Control::Idle};
	/// Sent to act on a thing (the tie, a tap on a thing, a field): what the leash sent it to do lasts while its mind is
	/// still to take the thing up or carries out the plan it made for it, not only while it moves
	bool sentToThing {false};

	/// The area the creature is kept within, none when the radius is 0: taken every turn from the hand's place on the
	/// ground or what the leash is tied to and the leash's full length, or from its home while it is young
	glm::vec3 confinementCentre {0.0f};
	float confinementRadius {0.0f};
	/// Where its home is, which it is kept at while it starts to grow up
	std::optional<glm::vec3> home;
	/// Walking back into the area it is kept within, until the walk is over, and the point it set off for
	bool returning {false};
	glm::vec3 returningTo {0.0f};
	/// Sent again to a point while it already obeyed, the turns it waits there once the walk is over before its mind
	/// takes over again
	std::optional<int32_t> waitAfterWalk;
	/// Sent to a point holding something, it puts the thing down there once the walk is over: still to do, then under
	/// way until its hands are free, before its mind takes over again
	enum class PutDownAfterWalk : uint8_t
	{
		None,
		Due,
		Started,
	};
	PutDownAfterWalk putDownAfterWalk {PutDownAfterWalk::None};
	/// Sent there holding something on the aggression leash, or where it can't stand, the point it throws the thing at
	/// instead of putting it down
	std::optional<glm::vec3> throwAt;
	/// The mark shown where the leash last sent it on the land, until it is done or the mark fades
	entt::entity targetMarker {entt::null};
	/// Where the leash last sent it: the point above its target (2 m over the land, over the thing's top for a thing),
	/// the thing it was sent to act on (none for a point of the land, and once the thing is gone), and whether it has a
	/// target, until it is done or the thing goes
	glm::vec3 targetPoint {0.0f};
	entt::entity targetThing {entt::null};
	bool hasTarget {false};
};

/// A sprite of the marker over where a creature was sent with the leash
struct LeashMarker
{
	entt::entity creature {entt::null};
};

/// Kept on a creature that another creature is leashed to: the game turn the two last came to find each other nicer or
/// nastier, 0 for never. Every creature leashed to it shares it, and untying the leash leaves it.
struct CreatureLeashAttitude
{
	uint32_t lastStep {0};
};

} // namespace openblack::ecs::components
