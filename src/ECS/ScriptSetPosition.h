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

#include <entt/fwd.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

namespace openblack
{
class LandIslandInterface;
}

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::components
{
struct CreatureAnimation;
}

/// What the scripts' SET_POSITION does to a thing (docs/wiki/scripts/script-commands.md#set_position-024). Pure rules: the
/// native gathers the facts and acts on the answer.
namespace openblack::ecs::script_set_position
{

/// How the thing is moved
enum class Move : uint8_t
{
	/// Nothing moves: no thing, held in a hand, flying, or an object out of the map
	Refused,
	/// A creature is put there at once, on the ground, by its locomotion; nothing else of the native follows
	Creature,
	/// A thing that is not an object (a script marker, a flock, a town...): its position is set, nothing else
	Copy,
	/// Any other object: moved in the map, stood upright on the ground and drawn there from the next frame
	Object,
};

/// What the native knows of the thing
struct Facts
{
	bool valid {false};
	bool inHand {false};
	bool flying {false};
	/// An object, as against a script marker, a flock, a town and the other things with a place
	bool object {false};
	bool inMap {false};
	bool creature {false};
	bool scriptMarker {false};
	bool living {false};
	bool controlledByScript {false};
};

/// The answer
struct Plan
{
	Move move {Move::Refused};
	/// The point keeps the height it was given; anything else goes on the ground
	bool keepsHeight {false};
	/// A living thing a script controls goes into script state 4 (in script) after the move
	bool intoScriptState {false};
};

/// The checks in the original's order: a thing in a hand or flying is refused before anything else; a thing that is not
/// an object is only given its position; an object out of the map is refused; a creature is placed and the native
/// returns, so it is never put into a script state; any other object is moved, and a living one under a script's
/// control goes into script state 4. Only a script marker keeps the height it was given
[[nodiscard]] constexpr Plan PlanFor(const Facts& facts)
{
	if (!facts.valid || facts.inHand || facts.flying)
	{
		return {};
	}
	if (!facts.object)
	{
		return {.move = Move::Copy, .keepsHeight = facts.scriptMarker};
	}
	if (!facts.inMap)
	{
		return {};
	}
	if (facts.creature)
	{
		return {.move = Move::Creature};
	}
	return {.move = Move::Object, .intoScriptState = facts.living && facts.controlledByScript};
}

/// Whether the native takes the thing to be flying, and so leaves it where it is: it is in the physics
/// (components::InPhysics), thrown or knocked and still moving, carried by a tornado or a vortex, or left hanging where a
/// hand let go of it. A body that has come to rest is not flying, though the physics keeps it to be knocked again
[[nodiscard]] bool IsFlying(const Registry& registry, entt::entity thing);

/// A creature's body as the native leaves it once the creature is put at the point: whatever it was playing (an action,
/// a dance, the walk's blend) stops at once and it stands, its face goes back to none at once, and its head looks
/// straight ahead, turned no more and not turning, at nothing. The breath, a gesture, the wobble and the sway play on
void StandStill(components::CreatureAnimation& animation);

/// An object's turn once it is moved: its matrix is built again from its Y angle alone, so a tilt about x or z is lost.
/// A turn that is already about Y alone is kept as it is, since it is that angle's own turn
[[nodiscard]] glm::mat3 UprightRotation(const glm::mat3& rotation);

/// SET_FOCUS on an object that is not living: the angle about Y that faces the target from where the object stands,
/// taken in whole map units across the land; its height does not count. The object is turned to it with its leans
/// kept (placed_turn::AnglesAfterScriptSet, placed_turn::SetAngles)
[[nodiscard]] float FocusYAngle(const glm::vec3& position, const glm::vec3& target);

/// The YPOS property: how high a thing at `position` stands above the land, the land's height taken at its point in
/// whole map units across, as a thing's place keeps it (the land at 0 without an island)
[[nodiscard]] float HeightAboveLand(const LandIslandInterface* island, const glm::vec3& position);
/// Where a thing at `position` goes when a script sets its YPOS: the same point across, that high above the land there
[[nodiscard]] glm::vec3 AtHeightAboveLand(const LandIslandInterface* island, const glm::vec3& position, float height);

} // namespace openblack::ecs::script_set_position
