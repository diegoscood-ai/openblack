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
#include <vector>

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
/// [0xEA9EF0..0xEA9EF8]: the hand's point of the previous call (x and z; y is written 0, 0x82786A / 0x827A1F), which
/// every call overwrites with the current one (0x827A12..0x827A2A), the first included
glm::vec3 g_PreviousPoint {0.0f};

/// One circle of InfluenceCircle's list ([0xEB9A14]): its player (+0x2C), centre (+0x04 / +0x0C) and radius (+0x10)
struct Circle
{
	PlayerNames player;
	glm::vec3 centre;
	float radius;
};

/// fn_00827210: the point is inside the circle when dx * dx + dz * dz < r * r (strictly, 0x82723C)
bool Inside(const glm::vec3& centre, float radius, const glm::vec3& point)
{
	const float dx = point.x - centre.x;
	const float dz = point.z - centre.z;
	return dx * dx + dz * dz < radius * radius;
}

/// GGame::Update3DInfluence 0x5552A0: InfluenceCircle::Reset, then for every player GetNextPlayer 0x5508A0 gives (it
/// stops at the player of type 3, the neutral one, 0x5508C6) a circle for the citadel when Citadel::GetInfluence
/// 0x464090 is not 0 (0x55530E) and one per town of its list (GPlayer+0xA50 by +0x75C) when Town +0x5C8 is not 0
/// (0x555354). openblack builds the same set from the registry instead of keeping the list.
std::vector<Circle> Circles()
{
	std::vector<Circle> circles;
	auto& registry = Locator::entitiesRegistry::value();
	const auto add = [&circles](PlayerNames player, const glm::vec3& centre, float radius) {
		if (radius == 0.0f || player == PlayerNames::NEUTRAL || static_cast<size_t>(player) >= k_Players)
		{
			return;
		}
		circles.push_back({player, centre, radius});
	};
	registry.Each<const Temple, const Transform>([&](entt::entity entity, const Temple& temple, const Transform& transform) {
		add(temple.owner, transform.position, influence::CitadelRadius(entity));
	});
	registry.Each<const Town, const TownInfluence, const Transform>(
	    [&](const Town& town, const TownInfluence& townInfluence, const Transform& transform) {
		    add(town.owner, transform.position, townInfluence.radius);
	    });
	return circles;
}

/// fn_008277B0(previous, current, player): the first circle of the player that has one of the two points inside and
/// the other outside, or none
const Circle* CrossedCircle(const std::vector<Circle>& circles, const glm::vec3& previous, const glm::vec3& current,
                            size_t player)
{
	for (const auto& circle : circles)
	{
		if (static_cast<size_t>(circle.player) == player &&
		    Inside(circle.centre, circle.radius, previous) != Inside(circle.centre, circle.radius, current))
		{
			return &circle;
		}
	}
	return nullptr;
}
} // namespace

bool influence::HandCrossedInfluence(const glm::vec3& handPosition)
{
	// fn_00827820: [0xEB9A6C] = 0 (0x82783D), then the per-player "the hand is inside one of this player's circles" bits
	// (0x82785D..0x82789A)
	const auto circles = Circles();
	std::array<bool, k_Players> inside {};
	for (const auto& circle : circles)
	{
		if (Inside(circle.centre, circle.radius, handPosition))
		{
			inside[static_cast<size_t>(circle.player)] = true;
		}
	}
	bool crossed = false;
	if (!g_HaveState)
	{
		// 0x8279F8..0x827A10: the first call only remembers them, [0xEB9A68] = 1
		g_WasInside = inside;
		g_HaveState = true;
	}
	else
	{
		for (size_t player = 0; player < k_Players; ++player)
		{
			if (inside[player] == g_WasInside[player])
			{
				continue; // 0x8278AF: unchanged
			}
			g_WasInside[player] = inside[player]; // 0x8278C7, before the search
			// 0x8278CE: fn_008277B0 looks for the player's circle whose edge the hand crossed between the previous point
			// and this one; none (a circle that appeared, grew or shrank under a still hand) makes no ripple and no sound
			if (CrossedCircle(circles, g_PreviousPoint, handPosition, player) == nullptr)
			{
				continue;
			}
			// 0x8278E0..0x8278EC: the ripple is only made when that circle's player has [0xEB9A1C] set, a per-map latch
			// that fn_00828A50 clears (from LH3DIsland::Create 0x803E85) and fn_00883120 sets when the player's
			// influence boundary graphic has faded in to 1 (0x8831AD); InfluenceCircle::Draw only reads it (0x826F15).
			// Then [0xEB9A6C] = 1 (0x8279E2). (aproximado) openblack draws no influence boundary, so the latch is taken
			// as set: a player who has a circle has shown it.
			// (pending) the ripple itself (fn_00827670 for the crossing point, fn_00827250: a 0x68-byte effect raised to
			// the land's altitude): openblack has no influence boundary graphics.
			crossed = true;
		}
	}
	// 0x827A12..0x827A2A: the point is remembered on every call
	g_PreviousPoint = glm::vec3(handPosition.x, 0.0f, handPosition.z);
	return crossed;
}

void influence::detail::ResetHandCrossing()
{
	g_WasInside = {};
	g_HaveState = false;
	g_PreviousPoint = glm::vec3(0.0f);
}

void influence::ProcessHandCrossing(const glm::vec3& handPosition)
{
	// 0x5E61B5..0x5E61C4: fn_00827820, then the sound only when it set [0xEB9A6C]
	if (!HandCrossedInfluence(handPosition))
	{
		return;
	}
	// 0x5E61C6..0x5E621E: GAudio::PlaySoundEffect 0x429E30 with bank InGame (GAudio+0x3AC), sample 52
	// G_HandThroughInfluence_01, no owner (+0x20 = 0), is3D 1 (+0x08), track 0 (+0x0C), at the hand's point
	// [0xE9A100]; mode and loops stay the ctor's (3 and 0) and the .sad overrides the mode with 1 (a new channel every
	// time) and gives volume 40 and min / max 100 / 300. Going in and coming out sound the same, and several players'
	// circles crossed at once still sound once.
	// (pending) GInterface::StartImmersion(6, 0x80000000) after it (0x5E6224): force feedback, not ported.
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), 52};
	options.owner = audio::Owner::None();
	options.is3D = true;
	options.track = false;
	options.position = handPosition;
	audio::PlaySoundEffect(options);
}
