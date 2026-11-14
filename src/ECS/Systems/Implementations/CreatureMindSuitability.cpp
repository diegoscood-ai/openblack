/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a desire may act on: the tests a thing passes for the desires of compassion, anger and curiosity, by its class
// and its info row, as the game's desire table asks them.

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>

#include "3D/CreatureBody.h"
#include "Creature/CreatureDesires.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFriends.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/Registry.h"
#include "ECS/ScriptTypes.h"
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

bool mind_detail::AttackableBy(const ecs::Registry& registry, entt::entity thing, entt::entity creature)
{
	if (thing == creature || !registry.Valid(thing))
	{
		return false;
	}
	// A tree and a rock always may be attacked
	if (registry.AllOf<Tree>(thing) || ecs::object_physics::IsRockObject(thing))
	{
		return true;
	}
	// Another creature not while either is the other's friend; a living thing not when dead; any object as its row says
	if (registry.AllOf<Creature>(thing) && Friends(registry, thing, creature))
	{
		return false;
	}
	if (registry.AnyOf<Villager, Animal, Creature>(thing) && LivingIsDead(registry, thing))
	{
		return false;
	}
	return ecs::script_type::IsObject(thing) &&
	       InfoAllows(registry, thing, [](const auto& row) { return row.canCreatureAttackMe; });
}

bool mind_detail::InspectableBy(const ecs::Registry& registry, entt::entity thing)
{
	if (!registry.Valid(thing) || !ecs::script_type::IsObject(thing))
	{
		return false;
	}
	if (registry.AnyOf<Villager, Animal, Creature>(thing) && LivingIsDead(registry, thing))
	{
		return false;
	}
	return InfoAllows(registry, thing, [](const auto& row) { return row.canCreatureInspectMe; });
}

bool mind_detail::DesireMayActOn(const ecs::Registry& registry, Desire desire, entt::entity thing, entt::entity creature)
{
	switch (desire)
	{
	case Desire::Compassion:
		return HelpableBy(registry, thing);
	case Desire::Anger:
		return AttackableBy(registry, thing, creature);
	case Desire::Curiosity:
		return InspectableBy(registry, thing);
	default:
		return false;
	}
}
