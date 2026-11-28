/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a desire may act on: the test each desire asks of a thing, by its class and its info row, as the game's desire
// table asks them.

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>

#include "3D/CreatureBody.h"
#include "Animals/AnimalRules.h"
#include "Creature/CreatureActivity.h"
#include "Creature/CreatureDesires.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFriends.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/MagicTree.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/PuzzleTotem.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/Registry.h"
#include "ECS/ScriptTypes.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Villager/VillagerDeath.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using creature_desires::Desire;

namespace
{
/// The rows of a creature's plan while it dies or lies dead for good
constexpr uint32_t k_DyingAction = 9;
constexpr uint32_t k_DeadForGoodAction = 261;

/// A creature is dead while its plan is dying or lying dead for good
bool CreatureIsDead(const ecs::Registry& registry, entt::entity creature)
{
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	const auto& current = mind != nullptr ? mind->planner.current : std::nullopt;
	return current.has_value() && (current->action == k_DyingAction || current->action == k_DeadForGoodAction);
}

/// A living thing is dead by its own rules: a villager's, an animal's, a creature's plan
bool LivingIsDead(const ecs::Registry& registry, entt::entity thing)
{
	if (registry.AllOf<Villager>(thing))
	{
		return ecs::villager::IsDead(thing);
	}
	if (registry.AllOf<Animal>(thing))
	{
		return ecs::animal_ai::IsDead(thing);
	}
	return registry.AllOf<Creature>(thing) && CreatureIsDead(registry, thing);
}

/// A creature's row of the game's creature table, none without the tables
const GCreatureInfo* CreatureInfoOf(const ecs::Registry& registry, entt::entity creature)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& rows = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(registry.Get<const Creature>(creature).species);
	return row < rows.size() ? &rows.at(row) : nullptr;
}

/// An object's flag of the info row: a creature's from the creature table, any other object's from its own row. An
/// object whose row openblack does not look up yet is let, as before these tests
template <typename Flag>
bool InfoAllows(const ecs::Registry& registry, entt::entity thing, Flag flag)
{
	if (registry.AllOf<Creature>(thing))
	{
		const auto* row = CreatureInfoOf(registry, thing);
		return row == nullptr || flag(*row) != 0;
	}
	const auto* info = ecs::fire::traits::InfoOf(thing);
	return info == nullptr || flag(*info) != 0;
}

/// Whether two creatures are each other's friends, as a script makes them
bool Friends(const ecs::Registry& registry, entt::entity first, entt::entity second)
{
	const auto lists = [&registry](entt::entity creature, entt::entity other) {
		const auto* friends = registry.TryGet<const CreatureFriends>(creature);
		return friends != nullptr && std::ranges::find(friends->friends, other) != friends->friends.end();
	};
	return lists(first, second) || lists(second, first);
}
} // namespace

bool mind_detail::HelpableBy(const ecs::Registry& registry, entt::entity thing)
{
	if (registry.AllOf<Villager>(thing))
	{
		return !registry.AllOf<AtHome>(thing) && !ecs::object_flags::IsCannotBePickedUp(thing);
	}
	return ecs::script_type::IsObject(thing);
}

namespace
{
/// The kind of a thing, by how the desires examine it: by the game's class of the thing
creature_activity::Thing KindOf(const ecs::Registry& registry, entt::entity thing)
{
	using creature_activity::Thing;
	if (registry.AllOf<Villager>(thing))
	{
		return Thing::Villager;
	}
	if (registry.AllOf<Animal>(thing))
	{
		return Thing::Animal;
	}
	if (registry.AllOf<Creature>(thing))
	{
		return Thing::Creature;
	}
	if (registry.AllOf<ecs::components::Spell>(thing))
	{
		return Thing::Spell;
	}
	if (registry.AnyOf<Tree, MagicTree>(thing))
	{
		return Thing::Tree;
	}
	// A bonfire, a piece knocked off a building and a dead or felled tree are rocks
	if (registry.AnyOf<DeadTree, FelledTree>(thing) || ecs::object_physics::IsRock(thing))
	{
		return Thing::Rock;
	}
	if (registry.AnyOf<MobileObject, Pot, Ball, Scaffold>(thing))
	{
		return Thing::MobileObject;
	}
	if (registry.AnyOf<MobileStatic, TeleportStone, StreetLantern>(thing))
	{
		return Thing::MobileStatic;
	}
	if (registry.AllOf<Field>(thing))
	{
		return Thing::Field;
	}
	if (registry.AllOf<FishFarm>(thing))
	{
		return Thing::FishFarm;
	}
	if (registry.AllOf<VillageTotem>(thing))
	{
		return Thing::TotemStatue;
	}
	if (registry.AllOf<PuzzleTotem>(thing) || (registry.AllOf<Abode>(thing) && ecs::abodes::TypeOf(thing) == AbodeType::Totem))
	{
		return Thing::Totem;
	}
	if (registry.AnyOf<Abode, Temple, WorshipSite, Feature, AnimatedStatic, Flowers, BigForest>(thing))
	{
		return Thing::Building;
	}
	if (registry.AllOf<TownDesireFlag>(thing))
	{
		return Thing::PlainObject;
	}
	return Thing::Other;
}

/// A magic mushroom or a toadstool: the feature of the magic mushroom, or a mobile object of either row
bool IsMushroom(const ecs::Registry& registry, entt::entity thing)
{
	if (const auto* feature = registry.TryGet<const Feature>(thing))
	{
		return feature->type == FeatureInfo::MagicMushroom;
	}
	const auto* mobile = registry.TryGet<const MobileObject>(thing);
	return mobile != nullptr &&
	       (mobile->type == MobileObjectInfo::MagicMushroom || mobile->type == MobileObjectInfo::Toadstool);
}

/// What the creature knows of a thing when a desire examines it. Whether the thing lets a creature act on it at all is
/// asked by the caller (mind_detail::LetsCreatureAct)
creature_activity::ThingFacts FactsOf(const ecs::Registry& registry, Desire desire, entt::entity thing, entt::entity creature)
{
	using creature_activity::Thing;
	const auto kind = KindOf(registry, thing);
	creature_activity::ThingFacts facts {
	    .kind = kind,
	    .attackable = InfoAllows(registry, thing, [](const auto& row) { return row.canCreatureAttackMe; }),
	    .playable = InfoAllows(registry, thing, [](const auto& row) { return row.canCreaturePlayWithMe; }),
	    .inspectable = InfoAllows(registry, thing, [](const auto& row) { return row.canCreatureInspectMe; }),
	    .dead = LivingIsDead(registry, thing),
	    .dancing = registry.AllOf<Dancer>(thing),
	    .pickable = !registry.AllOf<AtHome>(thing) && !ecs::object_flags::IsCannotBePickedUp(thing),
	    .itself = thing == creature,
	    .friendOfCreature = kind == Thing::Creature && Friends(registry, thing, creature),
	    .mushroom = desire == Desire::GetHigh && IsMushroom(registry, thing),
	};
	if (const auto* animal = registry.TryGet<const Animal>(thing))
	{
		facts.frightening = animals::FrightensCreatures(animal->type);
	}
	if (desire == Desire::Poo && kind != Thing::Spell && kind != Thing::Other)
	{
		facts.height = ecs::object::GetHeight(thing);
	}
	if (desire == Desire::BringStuffHome && Locator::creatureObjectActionSystem::has_value())
	{
		facts.creatureCanPickUp = Locator::creatureObjectActionSystem::value().CanPickUp(creature, thing);
	}
	return facts;
}
} // namespace

bool mind_detail::DesireMayActOn(const ecs::Registry& registry, Desire desire, entt::entity thing, entt::entity creature)
{
	return registry.Valid(thing) && creature_activity::ThingSuits(desire, FactsOf(registry, desire, thing, creature));
}
