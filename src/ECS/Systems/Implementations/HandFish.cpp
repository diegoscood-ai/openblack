/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand and the farms: the splash of gripping the sea, catching fish (FishFarm locked select) and taking the
// food of the fields (Field locked select)

#define LOCATOR_IMPLEMENTATIONS

#include "HandSystem.h"
#include "HandSystemDetail.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>

#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Audio/Audio.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Fields.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/FishShoals.h"
#include "ECS/WaterRings.h"
#include "Common/GameRandom.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "GameClock.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::hand_detail;

void HandSystem::GripLandSound(glm::vec3 point) noexcept
{
	// StartLandscapeGrip fn_005D1AB0 0x5D1FC4 (on land, unless the HelpSystem g_game+0x25005C has a script's
	// widescreen on, +0x45E8 and +0x45EC: see HandPlacement.cpp): SoundTag::Create(the grip's MapCoords,
	// GetRandomSample(4 G_HandGrabLand_01, 6) 0x71ED40, track 0, mode 3, loops 0, +0x40 0, is3D 0, InGame (ebx), delay 0)
	// 0x5D1FE4 -> fn_0071EA40 plays it at once with is3D 0: a 2D one-shot owned by the tag, vol 10, pitch 60 +-15 % (.sad
	// flags 0x3A1). The point tag has no thing and goes when the sample ends.
	audio::tags::Create(point, audio::tags::RandomSample(4, 6), false, 3, 0, false, false, audio::SfxBank::InGame, 0);
}

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
	// CRT Random(0, 2 pi) 0x5D2057, drawn before the ring pool is searched (0x5D205C): even when it is full
	ring.angle = game_random::crt::Random(0.0f, 6.2831853f);
	ring.cell = 0x30;
	ring.argb = 0xB0FFFFFFu;
	ring.seaLight = true;
	ecs::AddWaterRing(ring);
	// the sample (0x5D20E9..0x5D2167): bank InGame, 99 G_HandInWater_01 + the counter [0xD18228] (0..9 in turn,
	// advanced even when culled), is3D 1, +0x0C 0 (not moved with an object), no object, at (x, 0.2, z);
	// GAudio::PlaySoundEffect 0x429E30 does not start it farther than 150 from the camera. The ten are clone group 4 of
	// InGame.sad and play in the default mode 3 with no object, so LHSamplePlay restarts the channel of the previous one
	// (0x10011146..0x100111BC): one at a time.
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), 99 + audio::NextCounter(audio::Counter::HandInWater)};
	options.is3D = true;
	options.track = false;
	options.position = glm::vec3(point.x, 0.2f, point.z);
	audio::PlaySoundEffect(options);
	ecs::SplashWater(glm::vec3(point.x, 0.2f, point.z));
}

bool HandSystem::TryPickUpFish(entt::entity farm, glm::vec3 point) noexcept
{
	// FishFarm::NetworkFriendlyStartLockedSelect 0x52D770: min(amountPickedUpInitially, GetFoodValue(3)) of HandFood
	// in a Pot::Create at the status's MapCoords +0x14 (0x52D814), without taking it from the farm's stock, put in the
	// hand (0x52D840)
	if (!Locator::entitiesRegistry::value().AllOf<FishFarm>(farm))
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
	// GInterfaceStatus::PlaceObjectInMagicHand 0x5DC870 (not GenericPickup: no pick-up sound)
	PickUp(pile, false);
	_pickSource = farm;
	_pickFish = true;
	_pickTurns = 0;
	_pickLock = _interactionPoint.value_or(glm::vec3(point.x, 0.0f, point.z));
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Hand: catching fish from farm {}", static_cast<uint32_t>(farm));
	return true;
}

bool HandSystem::ProcessInInteractFish() noexcept
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
	const float t = static_cast<float>(_pickTurns) / 60.0f;
	auto take = static_cast<uint32_t>(8.0f + 62.0f * t * t);
	take = std::min(take, 1400u);
	take = std::min(take, pile->amount < 20000 ? 20000u - pile->amount : 0u);
	if (ecs::RemoveFishFarmFood(*_pickSource, take) == 0)
	{
		return false;
	}
	pile->amount = static_cast<uint16_t>(std::min<uint32_t>(pile->amount + take, 65535u));
	// UpdateMultiPickup(3, t^2): the looping G_PICKUPFOOD at 60 + 180 t^2 percent (UpdatePickupSound)
	_pickupSoundFraction = std::min(t, 1.0f) * std::min(t, 1.0f);
	registry.SetDirty();
	if (std::getenv("OPENBLACK_HAND_TRACE") != nullptr)
	{
		const auto& farm = registry.Get<const FishFarm>(*_pickSource);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fish trace: turn {} hand {} farm food {:.0f} shown {}", _pickTurns, pile->amount,
		                   farm.food, farm.VisibleFish());
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

std::optional<entt::entity> HandSystem::FishFarmUnderHand(glm::vec3 point) noexcept
{
	if (IsLand(point))
	{
		return std::nullopt;
	}
	return ecs::FindFishFarmAt(point);
}

bool HandSystem::FieldValidForLockedSelect(entt::entity field) noexcept
{
	const auto* data = Locator::entitiesRegistry::value().TryGet<const Field>(field);
	return data != nullptr && data->growth > 0.0f && data->food > 1.0f;
}

bool HandSystem::TryPickUpField(entt::entity field, std::optional<glm::vec3> at) noexcept
{
	// ValidForLockedSelectProcess 0x5299E0: growth > 0 and food > 1. NetworkFriendlyStartLockedSelect 0x529900:
	// n = (int)min(25, food), halved when ripe, taken from the field (unlike the fish farm) into a HandFood pot
	auto& registry = Locator::entitiesRegistry::value();
	if (!FieldValidForLockedSelect(field))
	{
		return false;
	}
	const auto* data = registry.TryGet<const Field>(field);
	auto n = static_cast<int>(std::min(Field::k_TakenWithHand, data->food));
	if (ecs::IsFieldRipe(field))
	{
		n /= 2;
	}
	if (n <= 0)
	{
		return false;
	}
	ecs::RemoveFieldFood(field, static_cast<float>(n));
	// Pot::Create at the status's MapCoords +0x14 (0x529994)
	const auto point = at.value_or(_interactionPoint.value_or(registry.Get<Transform>(field).position));
	const float ground = Locator::terrainSystem::value().GetHeightAt(glm::vec2(point.x, point.z));
	const auto pile = archetypes::PotArchetype::Create(glm::vec3(point.x, ground, point.z), 0.0f, PotInfo::HandFood, n);
	if (pile == entt::null)
	{
		return false;
	}
	// GInterfaceStatus::PlaceObjectInMagicHand 0x5DC870 (not GenericPickup: no pick-up sound)
	PickUp(pile, false);
	_pickSource = field;
	_pickField = true;
	_pickTurns = 0;
	_pickLock = _interactionPoint.value_or(glm::vec3(point.x, ground, point.z));
	return true;
}

bool HandSystem::ProcessInInteractField() noexcept
{
	// ProcessInInteract 0x529730, per game turn: n = (int)min(8 + 62 t^2, food), t = min(turns / 60, 1); at most the
	// room under 20000 in the hand; halved when ripe; RemoveFood(n) and the hand gets n (the original's quirk)
	auto& registry = Locator::entitiesRegistry::value();
	auto* pile = registry.TryGet<Pot>(*_held);
	const auto* field = registry.TryGet<const Field>(*_pickSource);
	if (pile == nullptr || field == nullptr)
	{
		return false;
	}
	const float t = std::min(static_cast<float>(_pickTurns) / 60.0f, 1.0f);
	_pickupSoundFraction = t * t;
	auto n = static_cast<int>(std::min(8.0f + 62.0f * t * t, field->food));
	n = std::min(n, 20000 - static_cast<int>(pile->amount));
	if (ecs::IsFieldRipe(*_pickSource))
	{
		n /= 2;
	}
	if (n <= 0)
	{
		return false;
	}
	ecs::RemoveFieldFood(*_pickSource, static_cast<float>(n));
	pile->amount = static_cast<uint16_t>(std::min<uint32_t>(pile->amount + static_cast<uint32_t>(n), 65535u));
	registry.SetDirty();
	field = registry.TryGet<const Field>(*_pickSource);
	if (std::getenv("OPENBLACK_HAND_TRACE") != nullptr && field != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Field trace: turn {} hand {} field food {:.0f} growth {:.0f} crops {}", _pickTurns,
		                   pile->amount, field->food, field->growth, field->crops);
	}
	return true;
}
