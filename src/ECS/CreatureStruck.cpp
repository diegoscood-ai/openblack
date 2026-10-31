/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureStruck.h"

#include <optional>

#include "Creature/CreatureDesires.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/PhysicsHooksWorld.h"
#include "Magic/MagicWorldInterface.h"
#include "Physics/LivingRules.h"

using namespace openblack;
using namespace openblack::ecs;
namespace living = openblack::physics::living;
namespace sources = openblack::creature_desires::sources;

bool creature_struck::IsToyModel(MeshId mesh)
{
	return mesh >= MeshId::ObjectToyBall && mesh <= MeshId::ObjectToySkittle;
}

bool creature_struck::IsToy(bool isFootball, std::optional<MeshId> staticModel)
{
	return isFootball || (staticModel.has_value() && IsToyModel(*staticModel));
}

void creature_struck::HurtCreature(physics_hooks::World& world, entt::entity creature, const Strike& strike)
{
	// Nothing struck it, or a toy did
	if (!strike.struck || (strike.hitter != entt::null && world.IsToy(strike.hitter)))
	{
		return;
	}
	// Any blow that counts sways it where the striking thing is, however soft, and even while a script holds it
	world.KickSway(creature, strike.force, strike.hitterCentre);
	// The creature of the player at this machine is not hurt at all while a script controls it
	const auto local = world.LocalPlayer();
	if (strike.scriptControlled && strike.owner == local)
	{
		return;
	}
	const float mass = living::CreatureMass(strike.drawnSize, strike.thinFat, strike.weakStrong);
	const auto crush = living::CreatureCrush(strike.impact, mass);
	if (!crush.has_value())
	{
		return;
	}
	// The crush, applied by what struck it and put down to the player who threw that, as a blow
	if (auto values = world.Crush())
	{
		values->Scale(*crush);
		const magic::EffectSource source {.player = strike.hitterPlayer.value_or(PlayerNames::NEUTRAL),
		                                  .appliedBy = strike.hitter,
		                                  .playerless = !strike.hitterPlayer.has_value(),
		                                  .blow = true,
		                                  .point = strike.hitterCentre};
		world.ApplyEffect(creature, *values, source);
	}
	// Struck by its own player's throw, it thinks less of the player
	if (strike.ownPlayer == strike.owner)
	{
		world.UpdateAttitudeFromFeedback(creature, -*crush);
	}
	world.ChangeDesireSource(creature, sources::k_FearFromDamage, *crush);
	world.ChangeDesireSource(creature, sources::k_AngerFromDamage, *crush);
}
