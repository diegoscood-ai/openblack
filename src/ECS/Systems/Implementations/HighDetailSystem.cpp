/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HighDetailSystem.h"

#include <cstdint>

#include "ECS/Registry.h"
#include "ECS/SuperVillager.h"
#include "GameClock.h"
#include "Locator.h"

using namespace openblack::ecs::systems;

namespace super_villager = openblack::ecs::super_villager;

void HighDetailSystem::Make(entt::entity thing)
{
	super_villager::SetHighGraphicsDetail(thing, true);
}

void HighDetailSystem::Release(entt::entity thing)
{
	// The script's off: the mark cleared and the draw list rebuilt before the thing leaves the list
	super_villager::SetHighGraphicsDetail(thing, false);
}

bool HighDetailSystem::Order(entt::entity thing, high_detail_rules::ThingSpecial special, bool on)
{
	// Whether it is drawn in high detail is read as the order is given. Every number a script gives reaches ours,
	// including the ones the enum does not name, and releasing the opening's specials needs no high-detail thing.
	const bool drawn = openblack::Locator::entitiesRegistry::value().AllOf<openblack::ecs::components::SuperVillager>(thing);
	super_villager::ThingJcSpecial(thing, static_cast<int32_t>(special), on);
	return drawn;
}

void HighDetailSystem::Update()
{
	// Ours also makes the swim rings, takes the on-screen test and sets this frame's smooth-drawing parameters here
	super_villager::Update();
}

void HighDetailSystem::PlaceEyes([[maybe_unused]] uint32_t drawTime, [[maybe_unused]] const glm::mat4& viewProjection)
{
	// Neither is read: ours steps the eyes by this frame's game milliseconds and took the on-screen test in Update
	super_villager::Draw(static_cast<int32_t>(openblack::game_clock::FrameGameMs()));
}
