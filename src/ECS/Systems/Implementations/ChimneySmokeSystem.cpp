/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ChimneySmokeSystem.h"

#include <algorithm>

#include "3D/L3DMesh.h"
#include "Common/GameRandom.h"
#include "ECS/Components/ChimneySmoke.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "GameClock.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using openblack::ecs::components::ChimneySmoke;
using openblack::ecs::components::Transform;

namespace
{
/// The C runtime's random numbers, not the game's synced ones
chimney_smoke::Random CrtRandom()
{
	return [](float a, float b) { return game_random::crt::Random(a, b); };
}

/// The most turns the hand's velocity catches up on at once
constexpr uint32_t k_MostTurnsCaughtUp = 10;
} // namespace

void ChimneySmokeSystem::Attach(entt::entity abode, const graphics::L3DMesh& mesh, const Transform& transform, bool workshop)
{
	const auto& point = mesh.GetChimneyPos();
	if (!point.has_value())
	{
		return;
	}
	// The chimney's top through the home's turn and size, at its place
	const auto chimney = transform.position + transform.rotation * (*point * transform.scale);
	Locator::entitiesRegistry::value().Assign<ChimneySmoke>(
	    abode,
	    chimney_smoke::Create(chimney, workshop ? chimney_smoke::k_WorkshopSmoke : chimney_smoke::k_HomeSmoke, CrtRandom()));
}

void ChimneySmokeSystem::UpdateHandWind()
{
	if (!Locator::handSystem::has_value() || !Locator::time::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto hands = Locator::handSystem::value().GetPlayerHands();
	const auto hand = hands[static_cast<size_t>(HandSystemInterface::Side::Left)];
	if (!registry.Valid(hand) || !registry.AllOf<Transform>(hand))
	{
		return;
	}
	// The hand is at the origin while it is not in the world
	const glm::vec3 position = registry.Get<Transform>(hand).position;
	if (position == glm::vec3(0.0f))
	{
		return;
	}

	// Once a turn, the velocity eases towards the hand's motion over that turn; turns the frames skipped are caught up
	// on, each with an even share of the motion
	const uint32_t turn = game_clock::Turn();
	if (!_handSeen)
	{
		_handVelocity = glm::vec3(0.0f);
		_lastHandPosition = position;
		_lastTurn = turn;
		_handSeen = true;
	}
	else if (turn != _lastTurn)
	{
		const uint32_t turns = std::min<uint32_t>(turn - _lastTurn, k_MostTurnsCaughtUp);
		const glm::vec3 moved = (position - _lastHandPosition) / static_cast<float>(turns);
		const auto millisecondsPerTurn = static_cast<float>(game_clock::MsPerTurn());
		for (uint32_t i = 0; i < turns; ++i)
		{
			_handVelocity = chimney_smoke::EaseHandVelocity(_handVelocity, moved, millisecondsPerTurn);
		}
		_lastHandPosition = position;
		_lastTurn = turn;
	}
	_handWind = chimney_smoke::WindOf(position, _handVelocity);
}

glm::vec3 ChimneySmokeSystem::Drift(const glm::vec3& chimney)
{
	return chimney_smoke::Drift(chimney, _handWind, CrtRandom());
}
