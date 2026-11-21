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

#include <entt/entity/fwd.hpp>

namespace openblack::ecs
{
class Registry;
}

/// The draw of an object out of the map: what the player's hand or a computer player's hand holds, what flies in the
/// physics, what a creature holds and what a tornado or a vortex carries are not drawn by their class's own draw but by
/// the object's draw out of the map, which lights them with the land's bilinear light at their place and the haze
/// (land_light::ObjectLightOf({})) and gives them its own colours (ColoursOf). Some classes keep a draw of their own
/// there (HasItsOwnDraw)
namespace openblack::ecs::out_of_map_draw
{

/// The colours the draw out of the map gives an object
enum class Colours : uint8_t
{
	/// Not drawn out of the map this frame, or its class draws itself there: its class's colours
	OwnDraw,
	/// The land light alone: no tint and no specular of its own
	LandLight,
	/// Burning: the fire's charring grey as the tint and the fire's glow as the specular
	Burning,
	/// A poisoned pot or pile without a fire: the poison's tint and specular
	Poisoned,
};

/// What decides the colours of an object's draw out of the map
struct Object
{
	/// Drawn out of the map this frame (DrawnOutOfMap)
	bool outOfMap {false};
	/// Its class has a draw out of the map of its own (HasItsOwnDraw)
	bool ownDraw {false};
	/// A pot or a pile (the classes that test the poison first)
	bool pot {false};
	bool poisoned {false};
	bool burning {false};
};

/// Out of the map, the shared draw's colours: with a fire the fire's pair, otherwise the land light alone; a poisoned
/// pot or pile without a fire takes the poison's pair instead
[[nodiscard]] Colours ColoursOf(const Object& object) noexcept;

/// Whether the object is drawn out of the map this frame: in the player's hand (components::HandDrawPose), in the
/// physics or held by a creature (components::PhysicsDrawPose, components::HeldByCreature), carried by a tornado or a
/// vortex (components::CarriedByTornado, components::ParticleDrawnMatrix), or held by a computer player's hand
/// (`computerHeld`, which the renderer keeps)
[[nodiscard]] bool DrawnOutOfMap(const Registry& registry, entt::entity entity, bool computerHeld);

/// Whether the object's class draws itself out of the map rather than through the shared draw: a tree, a spell seed,
/// a one-shot orb. And, (not read) whether their own draws take the colours the shared draw leaves: a creature (a
/// creature it holds is drawn with its own body's draw), a SuperVillager (its body's draw takes the light again on
/// every path) and a magic shield (its spell draws it)
[[nodiscard]] bool HasItsOwnDraw(const Registry& registry, entt::entity entity);

} // namespace openblack::ecs::out_of_map_draw
