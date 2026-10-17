/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a creature's mind does about the miracles cast near it: it runs from a nasty one or goes to look at it, goes to
// look at a nice one, and learns them by doing so. When it does so, and whether it learns, is the creatures' reaction
// handler's (CreatureReactions.cpp)

#define LOCATOR_IMPLEMENTATIONS

#include <utility>

#include <glm/vec2.hpp>

#include "Creature/CreatureDesires.h"
#include "Creature/CreatureMiracleReactions.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureReactionRules.h"
#include "CreatureMindSystem.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// Seeing a nasty miracle adds this much to its fear
constexpr float k_FearOfScaryMagic = 0.5f;

/// On the learning leash
bool OnRope(entt::entity creature)
{
	if (!Locator::leashSystem::has_value())
	{
		return false;
	}
	const auto& leash = Locator::leashSystem::value();
	return leash.IsLeashed(creature) && leash.TypeOf(creature) == LeashType::Rope;
}
} // namespace

void CreatureMindSystem::ReactToNastyMagic(entt::entity creature, const glm::vec3& point, std::optional<size_t> learn)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	const auto* body = std::as_const(registry).TryGet<const Creature>(creature);
	if (mind == nullptr || body == nullptr || !mind->desires.has_value())
	{
		return;
	}
	auto& desires = *mind->desires;
	creature_desires::ChangeSource(desires, creature_desires::sources::k_FearFromScaryMagic, k_FearOfScaryMagic);
	// (inferred) the fear the game reads here is the desire to be afraid, read after it grew
	const bool curious =
	    creature_reaction_rules::CuriousAboutNastyMagic(OnRope(creature), desires[creature_desires::Desire::Fear].value);
	const float height = body->size * creature_morph::k_HeightAtSizeOne;
	const auto random = [](uint32_t range) { return Random(range); };
	const glm::vec2 at(point.x, point.z);
	Replan(creature, creature_mind::Activity::Planned,
	       curious ? creature_mind::ExamineMiracle(at, height, random) : creature_mind::RunAwayFromMiracle(at, height, random));
	if (learn.has_value())
	{
		WatchMiracle(creature, *learn);
	}
}

void CreatureMindSystem::ReactToNiceMagic(entt::entity creature, const glm::vec3& point, std::optional<size_t> learn)
{
	const auto* body = std::as_const(Locator::entitiesRegistry::value()).TryGet<const Creature>(creature);
	if (body == nullptr)
	{
		return;
	}
	const auto random = [](uint32_t range) { return Random(range); };
	Replan(creature, creature_mind::Activity::Planned,
	       creature_mind::ExamineMiracle({point.x, point.z}, body->size * creature_morph::k_HeightAtSizeOne, random));
	if (learn.has_value())
	{
		WatchMiracle(creature, *learn);
	}
}
