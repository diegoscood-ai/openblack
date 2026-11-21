/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "OutOfMapDraw.h"

#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/ParticleDrawnMatrix.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Tree.h"
#include "ECS/Registry.h"
#include "ECS/SuperVillager.h"

namespace openblack::ecs::out_of_map_draw
{

Colours ColoursOf(const Object& object) noexcept
{
	if (!object.outOfMap || object.ownDraw)
	{
		return Colours::OwnDraw;
	}
	// a pot or a pile tests the poison first, and only without a fire; every other object (and a pot otherwise) the fire
	if (object.pot && object.poisoned && !object.burning)
	{
		return Colours::Poisoned;
	}
	return object.burning ? Colours::Burning : Colours::LandLight;
}

bool DrawnOutOfMap(const Registry& registry, entt::entity entity, bool computerHeld)
{
	return computerHeld || registry.AnyOf<components::HandDrawPose, components::PhysicsDrawPose, components::HeldByCreature,
	                                      components::CarriedByTornado, components::ParticleDrawnMatrix>(entity);
}

bool HasItsOwnDraw(const Registry& registry, entt::entity entity)
{
	return registry.AnyOf<components::Tree, components::SpellSeed, components::OneOffSpellSeed, components::Creature,
	                      components::SuperVillager, components::MagicShield>(entity);
}

} // namespace openblack::ecs::out_of_map_draw
