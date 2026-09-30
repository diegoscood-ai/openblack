/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Influence.h"

#include <algorithm>
#include <cmath>

#include "ECS/Components/InfluenceRing.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TownInfluence.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "InfluenceState.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// g_game+0x14 & 0x2000 (not part of a land: set once at start in the original)
bool g_everywhere = false;

/// fn_005CDBD0 + the `obj+0x24 & 4` test in CalculatePlayerRawInfluence: the ring's object is held in a hand
bool IsAttachedObjectInHand(const InfluenceRing& ring)
{
	if (ring.attached == entt::null || !Locator::handSystem::has_value())
	{
		return false;
	}
	const auto held = Locator::handSystem::value().GetHeldObject();
	return held.has_value() && *held == ring.attached;
}

/// fn_004630F0: the citadel gives its radius where it reaches (r > d)
float CitadelInfluenceAt(PlayerNames player, const glm::vec3& position)
{
	auto& registry = Locator::entitiesRegistry::value();
	float result = 0.0f;
	bool found = false;
	registry.Each<const Temple, const Transform>([&](entt::entity entity, const Temple& temple, const Transform& transform) {
		if (found || temple.owner != player)
		{
			return; // GPlayer+0xA48: one citadel per player
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

/// fn_007479E0 over the player's town list (GPlayer+0xA50): each town gives its radius where it reaches (d < r)
float TownsInfluenceAt(PlayerNames player, const glm::vec3& position)
{
	auto& registry = Locator::entitiesRegistry::value();
	float sum = 0.0f;
	registry.Each<const TownInfluence, const Transform>([&](const TownInfluence& town, const Transform& transform) {
		if (town.owner == player && influence::detail::DistanceXZ(transform.position, position) < town.radius)
		{
			sum += town.radius;
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

float DistanceXZ(const glm::vec3& a, const glm::vec3& b)
{
	return std::hypot(a.x - b.x, a.z - b.z);
}
} // namespace detail

float CalculatePlayerInfluence(PlayerNames player, const glm::vec3& position, [[maybe_unused]] CalcType type,
                               bool includeAllies)
{
	// Influence::CalculatePlayerInfluence 0x5CD170
	if (g_everywhere)
	{
		return 1.0f;
	}
	// Not ported: GPlayer+0x934 (only ever 0), the virtual influence of each of the player's interface statuses
	// (fn_0076D330, SET_VIRTUAL_INFLUENCE: the only reader of `type`)
	const float raw = CalculatePlayerRawInfluence(player, position);
	if (includeAllies && raw <= 0.0f)
	{
		// fn_005CD400: the first ally (GPlayer::IsAllied 0x64D5D0 and +0x950[ally] > 0.1) with raw influence > 0,
		// else 0. openblack has no alliances yet: 0.
		return 0.0f;
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
	// Influence::CalculatePlayerRawInfluence 0x5CD230. Not ported: the multiplayer rule (no citadel -> 0) and
	// CameraExclusion::InsideInclusion 0x455E20 (true unless a save's camera force field is on).
	if (g_everywhere)
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
		if (ring->anti)
		{
			// an anti ring of this player covering the point: nothing (IsInInfluence 0x5CDA60, dist <= radius)
			if (ring->player == player && detail::DistanceXZ(ring->position, position) <= ring->radius)
			{
				return 0.0f;
			}
		}
		else if (ring->player == player)
		{
			// InfluenceRing::CalculateInfluence 0x5CD900
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
	// Influence::IsInAntiInfluence 0x5CD490 (no held-object check here)
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto entity : detail::GlobalsOrDefault().rings)
	{
		const auto* ring = registry.TryGet<const InfluenceRing>(entity);
		if (ring != nullptr && ring->player == player && ring->anti &&
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
	// Influence::CalculateInfluenceOnRange 0x5CD560 with GInfluenceInfo 0xD17CC0 (0.4, 0.2, 0.2); its third argument
	// (an int) is not read
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
		// the double 0.2 at 0x8C7C68, not valueOfSmall (same value): from 0.2 down to 0 (the step up at outer is kept)
		return (1.0f - (distance - outer) / (radius - outer)) * 0.2f;
	}
	return 0.0f;
}

void ProcessTurn()
{
	// GGame::ProcessTurn 0x54E63C: InfluenceRing::ProcessRings. The towns' radii (Town::Process 0x747380) are
	// recomputed here too, once per turn, before anything reads them.
	ProcessTowns();
	ProcessRings();
}

void SetLandNumber(int32_t land)
{
	detail::Globals().landNumber = land;
}

int32_t LandNumber()
{
	return detail::GlobalsOrDefault().landNumber;
}

void SetTownInfluenceMultiplier(float multiplier)
{
	detail::Globals().townMultiplier = multiplier;
}

void SetPlayerInfluenceMultiplier(float multiplier)
{
	detail::Globals().playerMultiplier = multiplier;
}

float TownInfluenceMultiplier()
{
	return detail::GlobalsOrDefault().townMultiplier;
}

float PlayerInfluenceMultiplier()
{
	return detail::GlobalsOrDefault().playerMultiplier;
}

void SetInfluenceEverywhere(bool on)
{
	g_everywhere = on;
}

bool IsInfluenceEverywhere()
{
	return g_everywhere;
}
} // namespace openblack::influence
