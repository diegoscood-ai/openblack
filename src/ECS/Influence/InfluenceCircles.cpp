/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// InfluenceCircle (0x826C50..0x827040) and the hand's crossing of it (fn_00827820): the circles GGame::Update3DInfluence
// 0x5552A0 rebuilds every frame (one per citadel and per town with influence) and the ripple plus the sound a hand that
// crosses one makes (fn_0x005e5cd0 0x5E61B0). Milestone B8 of dev\tmp_dis\audio\PLAN.md (ui_creature.md §2.2).

#include <array>
#include <cstdint>

#include "Audio/Audio.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownInfluence.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Influence.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// The eight players of the original ([0xEB9A48] and [0xEB9A1C] are arrays of 8)
constexpr size_t k_Players = 8;

/// [0xEB9A48]: the hand was inside a circle of this player on the previous frame, and [0xEB9A68]: that array is filled
/// in. Statics of the process in the original too: InfluenceCircle::Reset (GGame::ClearMap) only empties the circle
/// list, nothing clears these.
std::array<bool, k_Players> g_WasInside {};
bool g_HaveState = false;

/// fn_00827210: the point is inside the circle when dx * dx + dz * dz < r * r (strictly, 0x82723C)
bool Inside(const glm::vec3& centre, float radius, const glm::vec3& point)
{
	const float dx = point.x - centre.x;
	const float dz = point.z - centre.z;
	return dx * dx + dz * dz < radius * radius;
}

/// GGame::Update3DInfluence 0x5552A0: InfluenceCircle::Reset, then for every player but the neutral one
/// (GetNextPlayer 0x5508A0) a circle for the citadel when Citadel::GetInfluence 0x464090 is not 0 (0x55530E) and one per
/// town of its list (GPlayer+0xA50 by +0x75C) when Town +0x5C8 is not 0 (0x555354). openblack builds the same set from
/// the registry instead of keeping the list.
std::array<bool, k_Players> HandInsideCircles(const glm::vec3& handPosition)
{
	std::array<bool, k_Players> inside {};
	auto& registry = Locator::entitiesRegistry::value();
	const auto mark = [&inside](PlayerNames player, const glm::vec3& centre, float radius, const glm::vec3& point) {
		const auto index = static_cast<size_t>(player);
		if (radius == 0.0f || player == PlayerNames::NEUTRAL || index >= k_Players)
		{
			return;
		}
		if (Inside(centre, radius, point))
		{
			inside[index] = true;
		}
	};
	registry.Each<const Temple, const Transform>([&](entt::entity entity, const Temple& temple, const Transform& transform) {
		mark(temple.owner, transform.position, influence::CitadelRadius(entity), handPosition);
	});
	registry.Each<const Town, const TownInfluence, const Transform>(
	    [&](const Town& town, const TownInfluence& townInfluence, const Transform& transform) {
		    mark(town.owner, transform.position, townInfluence.radius, handPosition);
	    });
	return inside;
}
} // namespace

void influence::ProcessHandCrossing(const glm::vec3& handPosition)
{
	// fn_00827820: the per-player "the hand is inside one of this player's circles" bits, then the ones that changed
	const auto inside = HandInsideCircles(handPosition);
	if (!g_HaveState)
	{
		// 0x8279F8..0x827A10: the first call only remembers them, [0xEB9A68] = 1
		g_WasInside = inside;
		g_HaveState = true;
		return;
	}
	bool crossed = false;
	for (size_t player = 0; player < k_Players; ++player)
	{
		if (inside[player] == g_WasInside[player])
		{
			continue; // 0x8278AF: unchanged
		}
		g_WasInside[player] = inside[player];
		// 0x8278CE..0x8279E2: fn_008277B0 finds the circle whose edge the hand crossed and the point on it; the ripple
		// is only made when that circle's player has drawn his circles ([0xEB9A1C], a per-map latch set by
		// InfluenceCircle::Draw 0x826F18 and by the boundary graphic's fade reaching 1, 0x8831AD), and then
		// [0xEB9A6C] = 1. (aproximado) openblack draws no influence boundary, so the latch is taken as set: a player
		// who has a circle has shown it.
		// (pending) the ripple itself (fn_00827250, a 0x68-byte effect at the crossing point raised to the land's
		// altitude): openblack has no influence boundary graphics.
		crossed = true;
	}
	if (!crossed)
	{
		return;
	}
	// 0x5E61C6..0x5E621E: GAudio::PlaySoundEffect 0x429E30 with bank InGame (GAudio+0x3AC), sample 52
	// G_HandThroughInfluence_01, no owner (+0x20 = 0), is3D 1 (+0x08), track 0 (+0x0C), at the hand's point
	// [0xE9A100]; mode and loops stay the ctor's (3 and 0) and the .sad overrides the mode with 1 (a new channel every
	// time) and gives volume 40 and min / max 100 / 300. Going in and coming out sound the same.
	// (pending) GInterface::StartImmersion(6, 0x80000000) after it (0x5E6224): force feedback, not ported.
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), 52};
	options.owner = audio::Owner::None();
	options.is3D = true;
	options.track = false;
	options.position = handPosition;
	audio::PlaySoundEffect(options);
}
