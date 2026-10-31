/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Influence.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>

#include "ECS/Components/Influence.h"
#include "ECS/Components/InfluenceRing.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VirtualInfluence.h"
#include "ECS/PlayerAlliances.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "GameClock.h"
#include "InfluenceState.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// What the influence keeps for its test hook, in the debug hooks' store (Locator::debugHooks)
struct InfluenceEverywhereDebugHooksState
{
	// OPENBLACK_INFLUENCE_EVERYWHERE: influence everywhere (not part of a land: set once at start in the original)
	bool everywhere {false};
};

InfluenceEverywhereDebugHooksState& InfluenceEverywhereDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("influence: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<InfluenceEverywhereDebugHooksState>();
}

/// The ring's object is held in a hand.
/// (approximate): any hand in the original; only openblack's local hand is checked
bool IsAttachedObjectInHand(const InfluenceRing& ring)
{
	if (ring.attached == entt::null || !Locator::handSystem::has_value())
	{
		return false;
	}
	const auto held = Locator::handSystem::value().GetHeldObject();
	return held.has_value() && *held == ring.attached;
}

/// The citadel gives its radius where it reaches (r > d)
float CitadelInfluenceAt(PlayerNames player, const glm::vec3& position)
{
	auto& registry = Locator::entitiesRegistry::value();
	float result = 0.0f;
	bool found = false;
	registry.Each<const Temple, const Transform>([&](entt::entity entity, const Temple& temple, const Transform& transform) {
		if (found || temple.owner != player)
		{
			return; // one citadel per player
		}
		found = true;
		const float radius = influence::CitadelRadius(entity);
		if (radius > influence::detail::DistanceXZ(transform.position, position))
		{
			result = radius;
		}
	});
	return result;
}

/// What the local player's hand keeps of their influence past the border, at a place outside their anti-influence
/// rings: measured from the interface's action point for the interface's own questions and any other while the game
/// runs, else from the place the hand was synced to. Nothing for any other player: openblack has one interface
std::optional<float> VirtualInfluenceAt(PlayerNames player, const glm::vec3& position, influence::CalcType type)
{
	if (!Locator::handSystem::has_value() || !Locator::playerSystem::has_value() ||
	    player != Locator::playerSystem::value().GetLocalPlayer())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto carrier = registry.Front<VirtualInfluence>();
	if (carrier == entt::null || influence::IsInAntiInfluence(player, position))
	{
		return std::nullopt;
	}
	const auto& hand = Locator::handSystem::value();
	auto from = hand.GetSyncedPlace();
	if (virtual_influence::MeasuresFromActionPoint(true, type == influence::CalcType::Interface, game_clock::IsPaused()))
	{
		const auto point = hand.GetActionPoint().value_or(glm::vec3(0.0f));
		from = map_coords::FromMetres({point.x, point.z});
	}
	return virtual_influence::Grant(registry.Get<const VirtualInfluence>(carrier).state, from,
	                                map_coords::FromMetres({position.x, position.z}));
}

/// Over the player's towns: each town gives its radius where it reaches (d < r)
float TownsInfluenceAt(PlayerNames player, const glm::vec3& position)
{
	auto& registry = Locator::entitiesRegistry::value();
	float sum = 0.0f;
	registry.Each<const Town, const TownInfluence, const Transform>(
	    [&](const Town& town, const TownInfluence& influence, const Transform& transform) {
		    if (town.owner == player && influence::detail::DistanceXZ(transform.position, position) < influence.radius)
		    {
			    sum += influence.radius;
		    }
	    });
	return sum;
}
} // namespace

namespace openblack::influence
{
namespace detail
{
InfluenceGlobals& Globals()
{
	auto& registry = Locator::entitiesRegistry::value();
	auto entity = registry.Front<InfluenceGlobals>();
	if (entity == entt::null)
	{
		entity = registry.Create();
		return registry.Assign<InfluenceGlobals>(entity);
	}
	return registry.Get<InfluenceGlobals>(entity);
}

MapScriptGlobals& MapGlobals()
{
	return Locator::mapScriptSystem::value().Globals();
}

const InfluenceGlobals& GlobalsOrDefault()
{
	static const InfluenceGlobals k_Defaults {};
	if (!Locator::entitiesRegistry::has_value())
	{
		return k_Defaults;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Front<InfluenceGlobals>();
	return entity == entt::null ? k_Defaults : registry.Get<const InfluenceGlobals>(entity);
}

InfluenceGlobals* TryGlobals()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Front<InfluenceGlobals>();
	return entity == entt::null ? nullptr : registry.TryGet<InfluenceGlobals>(entity);
}

float DistanceXZ(const glm::vec3& a, const glm::vec3& b)
{
	return std::hypot(a.x - b.x, a.z - b.z);
}
} // namespace detail

float CalculatePlayerInfluence(PlayerNames player, const glm::vec3& position, CalcType type, bool includeAllies)
{
	if (InfluenceEverywhereDebugHooksData().everywhere)
	{
		return 1.0f;
	}
	// Not ported: a player field that is only ever 0. Then what the hand keeps of the player's influence past the border,
	// the only reader of `type`, before their own influence
	if (const auto granted = VirtualInfluenceAt(player, position, type); granted.has_value())
	{
		return *granted;
	}
	const float raw = CalculatePlayerRawInfluence(player, position);
	if (includeAllies && raw <= 0.0f)
	{
		// The first ally, in player slot order, whose influence the player may use (an alliance strength above 0.1) and
		// whose raw influence there is above 0; else 0
		return ecs::alliances::AlliesInfluence(ecs::alliances::AlliancesOf(player), [&position](PlayerNames ally) {
			return CalculatePlayerRawInfluence(ally, position);
		});
	}
	return raw;
}

float CalculatePlayerInfluence(entt::entity player, const glm::vec3& position, CalcType type, bool includeAllies)
{
	const auto* component = Locator::entitiesRegistry::value().TryGet<const Player>(player);
	if (component == nullptr)
	{
		return 0.0f; // no player: 0
	}
	return CalculatePlayerInfluence(component->name, position, type, includeAllies);
}

float CalculatePlayerRawInfluence(PlayerNames player, const glm::vec3& position)
{
	// Not ported: the multiplayer rule (no citadel -> 0) and the camera inclusion test (true unless a save's camera
	// force field is on).
	if (InfluenceEverywhereDebugHooksData().everywhere)
	{
		return 1.0f;
	}
	float sum = CitadelInfluenceAt(player, position) + TownsInfluenceAt(player, position);
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto entity : detail::GlobalsOrDefault().rings)
	{
		const auto* ring = registry.TryGet<const InfluenceRing>(entity);
		if (ring == nullptr || IsAttachedObjectInHand(*ring))
		{
			continue;
		}
		if (ring->anti != 0)
		{
			// an anti ring of this player covering the point: nothing (dist <= radius)
			if (ring->player == player && detail::DistanceXZ(ring->position, position) <= ring->radius)
			{
				return 0.0f;
			}
		}
		else if (ring->player == player)
		{
			sum += CalculateInfluenceOnRange(detail::DistanceXZ(ring->position, position), ring->radius);
		}
	}
	return std::clamp(sum, -1.0f, 1.0f);
}

bool IsInPlayerRawInfluence(PlayerNames player, const glm::vec3& position)
{
	return CalculatePlayerRawInfluence(player, position) > 0.0f;
}

bool IsInAntiInfluence(PlayerNames player, const glm::vec3& position)
{
	// No held-object check here
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto entity : detail::GlobalsOrDefault().rings)
	{
		const auto* ring = registry.TryGet<const InfluenceRing>(entity);
		if (ring != nullptr && ring->player == player && ring->anti != 0 &&
		    detail::DistanceXZ(ring->position, position) <= ring->radius)
		{
			return true;
		}
	}
	return false;
}

bool IsInAntiInfluence(entt::entity player, const glm::vec3& position)
{
	const auto* component = Locator::entitiesRegistry::value().TryGet<const Player>(player);
	return component != nullptr && IsInAntiInfluence(component->name, position);
}

float CalculateInfluenceOnRange(float distance, float radius)
{
	return CalculateInfluenceOnRange(distance, radius, Locator::infoConstants::value().influence);
}

float CalculateInfluenceOnRange(float distance, float radius, const GInfluenceInfo& info)
{
	// The influence info is (0.4, 0.2, 0.2); the original's third argument (an int) is not read
	const float full = info.percentageFullInfluence * radius;
	if (distance <= full)
	{
		return 1.0f;
	}
	const float outer = (info.percentageDistanceForDecreasingGradient + info.percentageFullInfluence) * radius;
	if (distance <= outer)
	{
		return (1.0f - (distance - full) / (outer - full)) * (1.0f - info.valueOfSmall);
	}
	if (distance < radius)
	{
		// a constant 0.2, not valueOfSmall (same value): from 0.2 down to 0 (the step up at outer is kept)
		return (1.0f - (distance - outer) / (radius - outer)) * 0.2f;
	}
	return 0.0f;
}

void ProcessTurn()
{
	// The turn's ring update. The towns' radii are recomputed here too, once per turn, before anything reads them,
	// and the citadels' part of their update
	ProcessTowns();
	ProcessCitadels();
	ProcessRings();
}

int32_t LandNumber()
{
	return detail::MapGlobals().landNumber;
}

float TownInfluenceMultiplier()
{
	return detail::MapGlobals().townInfluenceMultiplier;
}

float PlayerInfluenceMultiplier()
{
	return detail::MapGlobals().playerInfluenceMultiplier;
}

void SetInfluenceEverywhere(bool on)
{
	InfluenceEverywhereDebugHooksData().everywhere = on;
}

bool IsInfluenceEverywhere()
{
	return InfluenceEverywhereDebugHooksData().everywhere;
}
} // namespace openblack::influence
