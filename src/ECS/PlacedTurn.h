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

#include <entt/entity/fwd.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::components
{
struct TurnAngles;
}

/// How a placed thing turns and leans (docs/wiki/world/objects.md#angles-of-a-thing). The game keeps the angles of a
/// placed thing apart from its turn and makes its turn from them. A thing that can lean keeps all three angles and turns
/// about z, then x, then y; any other thing keeps only its angle about y and stands upright on it.
namespace openblack::ecs::placed_turn
{

/// Whether a thing can lean about x and z as well as turn about the upright axis: the mobile statics (rocks, bonfires,
/// fragments, dead and felled trees, teleport stones, landscape vortices, but the singing stone's base, which stands
/// upright) and the mobile objects (piles and pots, the football, sharks, one-off spell seeds, rewards, scaffolds and
/// the other mobile objects). All other things stand upright: their angles about x and z are always 0.
[[nodiscard]] bool CanLean(const Registry& registry, entt::entity entity);

/// The angles that a thing keeps from the given angles. A thing that does not lean keeps only the angle about y.
[[nodiscard]] constexpr glm::vec3 KeptAngles(const glm::vec3& angles, bool canLean)
{
	return canLean ? angles : glm::vec3(0.0f, angles.y, 0.0f);
}

/// The turn the game makes from kept angles: about z, then x, then y for a thing that leans, about y alone otherwise
[[nodiscard]] glm::mat3 TurnOf(const glm::vec3& kept, bool canLean);

/// The angles the game finds in a turn, taken off in the reverse order of its turn, and kept as a thing keeps them
[[nodiscard]] glm::vec3 AnglesOfTurn(const glm::mat3& turn, bool canLean);

/// The angles of a thing with this turn. The kept angles are the thing's own while it has the turn they were kept with;
/// when another turn replaced it, the angles come from the turn, as the game finds the angles of a matrix.
[[nodiscard]] glm::vec3 AnglesOf(const components::TurnAngles* kept, const glm::mat3& turn, bool canLean);

/// The angles of a thing, in radians about x, y and z, as the scripts read them. 0 for a thing with no turn.
[[nodiscard]] glm::vec3 AnglesOf(const Registry& registry, entt::entity entity);

/// The thing keeps these angles with the turn it has now. A thing that does not lean keeps only the angle about y.
/// Nothing for a thing with no turn.
void KeepAngles(Registry& registry, entt::entity entity, const glm::vec3& angles);

/// The thing is turned to the given angles with the game's turn, its scale made even, and keeps the angles: what the
/// game does when a thing's angles are set. A thing that does not lean keeps only the angle about y. False for a thing
/// with no turn (nothing is changed).
bool SetAngles(Registry& registry, entt::entity entity, const glm::vec3& angles);

/// The thing stands upright on its angle about y, its scale made even, as the game builds its matrix again when it is
/// moved: its leans stay kept, so they come back when one of its angles is set again. A thing whose kept angles are no
/// longer its own stands on the angle about y of its turn.
void StandUpright(Registry& registry, entt::entity entity);

/// One of a thing's three angles
enum class Axis : uint8_t
{
	X,
	Y,
	Z,
};

/// The angles a script's SET of one angle gives a thing that has `angles`: that angle replaced, the other two as they
/// are. None for the angle about y when the thing has it already: the original then leaves the thing alone. A SET of a
/// lean is always made, even with the same value
[[nodiscard]] constexpr std::optional<glm::vec3> AnglesAfterScriptSet(const glm::vec3& angles, Axis axis, float radians)
{
	switch (axis)
	{
	case Axis::X:
		return glm::vec3(radians, angles.y, angles.z);
	case Axis::Y:
		if (angles.y == radians)
		{
			return std::nullopt;
		}
		return glm::vec3(angles.x, radians, angles.z);
	case Axis::Z:
		return glm::vec3(angles.x, angles.y, radians);
	}
	return std::nullopt;
}

/// The angles of a thing after a turn about the upright axis by `radians`, in the direction of a world turn about y:
/// the game takes the turn off the angle about y and keeps the leans
[[nodiscard]] constexpr glm::vec3 TurnedAboutUpright(const glm::vec3& angles, float radians)
{
	return {angles.x, angles.y - radians, angles.z};
}

} // namespace openblack::ecs::placed_turn
