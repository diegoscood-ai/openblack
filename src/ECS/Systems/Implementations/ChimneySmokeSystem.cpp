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

#include "3D/L3DMesh.h"
#include "Common/GameRandom.h"
#include "ECS/Components/ChimneySmoke.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
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
	if (!Locator::handSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& hand = Locator::handSystem::value();
	const auto hands = hand.GetPlayerHands();
	const auto left = hands[static_cast<size_t>(HandSystemInterface::Side::Left)];
	if (!registry.Valid(left) || !registry.AllOf<Transform>(left))
	{
		return;
	}
	// The hand is at the origin while it is not in the world
	const glm::vec3 position = registry.Get<Transform>(left).position;
	if (position == glm::vec3(0.0f))
	{
		return;
	}
	// The air goes where the drawn hand is, at the synced hand's velocity, which eases towards its move once a turn
	_handWind = chimney_smoke::WindOf(position, hand.GetTurnHandVelocity());
}

glm::vec3 ChimneySmokeSystem::Drift(const glm::vec3& chimney)
{
	return chimney_smoke::Drift(chimney, _handWind, CrtRandom());
}
