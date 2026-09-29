/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand and the fish farms: the splash of gripping the sea and catching fish (FishFarm locked select)

#define LOCATOR_IMPLEMENTATIONS

#include "HandSystem.h"
#include "HandSystemDetail.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>

#include <spdlog/spdlog.h>

#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Pot.h"
#include "ECS/FishShoals.h"
#include "ECS/WaterRings.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::hand_detail;

namespace
{
constexpr std::array k_HandInWater = {
    audio::SoundId::G_HandInWater_01, audio::SoundId::G_HandInWater_02, audio::SoundId::G_HandInWater_03,
    audio::SoundId::G_HandInWater_04, audio::SoundId::G_HandInWater_05, audio::SoundId::G_HandInWater_06,
    audio::SoundId::G_HandInWater_07, audio::SoundId::G_HandInWater_08, audio::SoundId::G_HandInWater_09,
    audio::SoundId::G_HandInWater_10,
};
} // namespace

void HandSystem::SplashHand(glm::vec3 point) noexcept
{
	// StartLandscapeGrip fn_005D1AB0: gripping the water (or off the map) splashes at (x, 0.2, z) with the next of the
	// ten LH_SAMPLE_G_HANDINWATER samples in turn (counter 0xD18228) and a water ring: growth 7, a random angle,
	// cell 0x30, colour 0xB0 alpha with the full light of the landscape light table
	if (IsLand(point))
	{
		return;
	}
	ecs::WaterRing ring;
	ring.position = glm::vec3(point.x, 0.2f, point.z);
	ring.growth = 7.0f;
	ring.angle = Locator::rng::value().NextValue(0.0f, 6.2831853f);
	ring.cell = 0x30;
	ring.argb = 0xB0FFFFFFu;
	ring.seaLight = true;
	ecs::AddWaterRing(ring);
	static size_t next = 0;
	PlaySample(k_HandInWater.at(next));
	next = (next + 1) % k_HandInWater.size();
	ecs::SplashWater(glm::vec3(point.x, 0.2f, point.z));
}

bool HandSystem::TryPickUpFish(glm::vec3 point) noexcept
{
	// FindObjectNearMapCoord 0x5D39E0: over the water with nothing else under the hand, a shown fish within 2 units
	// makes its FishFarm the object of the action; FishFarm::NetworkFriendlyStartLockedSelect 0x52D770 then puts
	// amountPickedUpInitially of HandFood in the hand, without taking it from the farm's stock
	if (IsLand(point))
	{
		return false;
	}
	const auto farm = ecs::FindFishFarmAt(point);
	if (!farm)
	{
		return false;
	}
	const auto& info = Locator::infoConstants::value().pot[static_cast<size_t>(PotInfo::HandFood)];
	const auto amount = std::min<uint32_t>(info.amountPickedUpInitially, static_cast<uint32_t>(FishFarm::k_FoodValue));
	const auto pile = archetypes::PotArchetype::Create(glm::vec3(point.x, 0.0f, point.z), 0.0f, PotInfo::HandFood,
	                                                   static_cast<int32_t>(amount));
	if (pile == entt::null)
	{
		return false;
	}
	PickUp(pile);
	_pickSource = *farm;
	_pickFish = true;
	_pickTurns = 0;
	_pickTurnAccumulator = 0.0f;
	_pickLock = glm::vec3(point.x, 0.0f, point.z);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Hand: catching fish from farm {}", static_cast<uint32_t>(*farm));
	return true;
}

bool HandSystem::UpdateFishPickUp(float seconds) noexcept
{
	// FishFarm::ProcessInInteract 0x52D950, once per game turn: n = (int)(8 + 62 t^2), t = turns / 60, at most 1400
	// and the room left under 20000 in the hand; RemoveFood gives what the farm has, but the hand gets n (a quirk of
	// the original). Nothing left: the select ends.
	auto& registry = Locator::entitiesRegistry::value();
	auto* pile = registry.TryGet<Pot>(*_held);
	if (pile == nullptr || !registry.AllOf<FishFarm>(*_pickSource))
	{
		return false;
	}
	constexpr float k_TurnSeconds = 0.1f;
	_pickTime += seconds;
	_pickTurnAccumulator += seconds;
	bool changed = false;
	while (_pickTurnAccumulator >= k_TurnSeconds)
	{
		_pickTurnAccumulator -= k_TurnSeconds;
		++_pickTurns;
		const float t = static_cast<float>(_pickTurns) / 60.0f;
		auto take = static_cast<uint32_t>(8.0f + 62.0f * t * t);
		take = std::min(take, 1400u);
		take = std::min(take, pile->amount < 20000 ? 20000u - pile->amount : 0u);
		if (ecs::RemoveFishFarmFood(*_pickSource, take) == 0)
		{
			_pickSource.reset();
			_pickFish = false;
			break;
		}
		pile->amount = static_cast<uint16_t>(std::min<uint32_t>(pile->amount + take, 65535u));
		changed = true;
		// LH_SAMPLE_G_PICKUPFOOD (pitch 60 + 180 t^2 in the original; no pitch control here)
		PlaySample(audio::SoundId::G_PickUpFood);
	}
	if (changed)
	{
		registry.SetDirty();
	}
	if (std::getenv("OPENBLACK_HAND_TRACE") != nullptr && changed)
	{
		const auto* farm = _pickSource ? registry.TryGet<FishFarm>(*_pickSource) : nullptr;
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fish trace: turn {} hand {} farm food {:.0f} shown {}", _pickTurns, pile->amount,
		                   farm != nullptr ? farm->food : -1.0f, farm != nullptr ? farm->VisibleFish() : 0);
	}
	return true;
}

void HandSystem::UpdateTestSplash(float seconds) noexcept
{
	static const char* at = std::getenv("OPENBLACK_TEST_SPLASH");
	static float timer = 0.0f;
	float x = 0.0f;
	float z = 0.0f;
	if (at == nullptr || std::sscanf(at, "%f,%f", &x, &z) != 2)
	{
		return;
	}
	timer -= seconds;
	if (timer <= 0.0f)
	{
		timer = 1.0f;
		SplashHand(glm::vec3(x, 0.0f, z));
	}
}
