/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MimicType.h"

#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FootballPitch.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Components/MagicTree.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/PuzzleTotem.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Villager/VillagerCore.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;

creature_mimic_type::MimicType ecs::MimicTypeOf(const Registry& registry, entt::entity thing)
{
	if (!registry.Valid(thing))
	{
		return creature_mimic_type::MimicType::Other;
	}
	creature_mimic_type::Facts facts;
	facts.creature = registry.AllOf<Creature>(thing);
	facts.villager = registry.AllOf<Villager>(thing);
	if (facts.villager)
	{
		facts.child = villager::IsChild(thing);
		facts.female = villager::IsWoman(thing);
	}
	facts.animal = registry.AllOf<Animal>(thing);
	if (facts.animal && Locator::dynamicsSystem::has_value())
	{
		const auto* info = Locator::dynamicsSystem::value().ObjectInfo(thing);
		facts.nastyAnimal = info != nullptr && info->alignmentType == AlignmentType::AnimalNasty;
	}
	// the game's rocks: a rock, the bonfire, a piece knocked off a building, and a dead or felled tree
	facts.rock =
	    object_physics::IsRockObject(thing) || object_physics::IsBonfire(thing) || registry.AnyOf<Fragment, DeadTree>(thing);
	facts.tree = registry.AnyOf<Tree, MagicTree>(thing);
	// the game's abodes: the buildings, and the fields, the football pitches, the totems and the spell dispensers
	facts.abode = registry.AnyOf<Abode, Field, FootballPitch, VillageTotem, PuzzleTotem, SpellDispenser>(thing);
	facts.mobile =
	    registry.AnyOf<MobileObject, MobileStatic, Pot, Reward, Scaffold, Ball, LandscapeVortex, OneOffSpellSeed>(thing);
	return creature_mimic_type::Of(facts);
}
