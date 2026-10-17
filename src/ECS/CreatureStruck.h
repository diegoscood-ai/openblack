/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "3D/AllMeshes.h"
#include "Enums.h"

namespace openblack::ecs::physics_hooks
{
class World;
}

/// A creature struck by a thrown thing. Unless a toy struck it, the turn's force on its body sets it swaying where the
/// striking thing is. Then, unless a script controls it and it is the creature of the player at this machine, it takes
/// the crush effect by how hard it was struck for its weight, as long as that is enough to count. Its own player's
/// throw makes it think less of the player, and every such blow makes it more afraid, then angrier.
namespace openblack::ecs::creature_struck
{

/// The effect info's row of the crush a thing in the physics does to what it strikes
inline constexpr size_t k_CrushEffect = 3;

/// Whether a static's model is one of the toys: the ball, the bowling ball, the cuddly toy, the dice or the skittle
[[nodiscard]] bool IsToyModel(MeshId mesh);

/// What the physics and the creature give of a strike, at the end of the turn
struct Strike
{
	/// Something's body struck the creature's this turn
	bool struck {false};
	/// The thing whose body struck it, null when that thing has gone; whether it is a creature, and where its body's
	/// centre is
	entt::entity hitter {entt::null};
	bool hitterIsCreature {false};
	glm::vec3 hitterCentre {0.0f};
	/// The player's hand threw what struck it, directly or through what that struck
	bool hitterByPlayer {false};
	/// The creature's own body, which takes that from what struck it
	bool ownByPlayer {false};
	/// The force on the creature's body over the turn, and the turn's mean force
	glm::vec3 force {0.0f};
	float impact {0.0f};
	/// The creature's player, and whether a script controls it
	PlayerNames owner {PlayerNames::NEUTRAL};
	bool scriptControlled {false};
	/// The size its body is drawn at, and how thin or fat and how weak or strong it is to become, each -1 to 1
	float drawnSize {1.0f};
	float thinFat {0.0f};
	float weakStrong {0.0f};
};

/// The creature takes the strike: the sway, the crush, its player's feedback, its fear and then its anger, each from
/// the world
void HurtCreature(physics_hooks::World& world, entt::entity creature, const Strike& strike);

} // namespace openblack::ecs::creature_struck
