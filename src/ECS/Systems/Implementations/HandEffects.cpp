/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <fstream>
#include <tuple>

#include <L3DFile.h>
#include <LNDFile.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Audio.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Common/GameRandom.h"
#include "Debug/DebugEnv.h"
#include "Debug/TraceLog.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/HandArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Hand/HandPickUpRules.h"
#include "Hand/HandSoundRules.h"
#include "HandSystem.h"
#include "HandSystemDetail.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::hand_detail;

void HandSystem::UpdatePickupParticles(bool emitting) noexcept
{
	// The batch pick-up's pieces are the particle system's effect of their kind, started as the batch begins (the old
	// one killed first) and killed as it ends; the effect steps itself every frame with the hand as its gesture position
	if (emitting == _pickupStarted || !Locator::particleSystem::has_value())
	{
		return;
	}
	_pickupStarted = emitting;
	auto& particles = Locator::particleSystem::value();
	if (!emitting)
	{
		particles.StopMultiPickup();
		return;
	}
	const auto* pot = Locator::entitiesRegistry::value().TryGet<const Pot>(*_held);
	const auto type = PotInfoOf(*_held) == PotInfo::HandWood ? ParticleType::WoodPickup
	                  : _pickFish                            ? ParticleType::FoodPickupFish
	                  : pot != nullptr && pot->poisoned      ? ParticleType::FoodPickupPoisoned
	                                                         : ParticleType::FoodPickup;
	particles.StartMultiPickup(type);
}

void HandSystem::UpdatePickupSound(bool active) noexcept
{
	// the pick-up plays 98 G_PickUpWood for a wood pile and 44 G_PickUpFood for everything else
	// (food piles, fields, fish farms) every turn: bank InGame, 3D, not
	// moved afterwards, no object, at the hand's position as the interface status last received it (the hand sends it
	// every turn, before the pick-up is updated).
	// The .sad (flags 0x7E0) makes it loop (-1) in mode 2, so
	// after the first turn playing again leaves the playing channel (and its position) alone and only
	// the pitch (60 + 180 t^2, truncated) changes it (rate * p / 100, from 60 % to 240 %).
	// Stopping the pick-up stops both samples, 44 and 98.
	constexpr int k_Food = hand_pick_up::k_PickUpFoodSample; // G_PickUpFood
	constexpr int k_Wood = hand_pick_up::k_PickUpWoodSample; // G_PickUpWood
	const auto none = audio::Owner::None();
	if (!active)
	{
		if (_pickupSound)
		{
			// stopping the pick-up stops both samples
			audio::StopSoundEffect(k_Food, none, audio::SfxBank::InGame);
			audio::StopSoundEffect(k_Wood, none, audio::SfxBank::InGame);
			_pickupSound.reset();
		}
		_pickupSoundFraction = 0.0f;
		return;
	}
	const int sample = hand_pick_up::PickUpSoundSample(PotInfoOf(*_held) == PotInfo::HandWood);
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), sample};
	options.is3D = true;
	options.track = false;
	options.position = Locator::entitiesRegistry::value().Get<Transform>(_hands[static_cast<size_t>(Side::Left)]).position;
	const auto channel = audio::PlaySoundEffect(options);
	if (channel != audio::k_NoChannel)
	{
		_pickupSound = channel;
	}
	// the pitch: 60 + 180 x the stored fraction (t^2), truncated
	audio::SetPitch(audio::Bank(audio::SfxBank::InGame), none, sample, hand_pick_up::PickUpSoundPitch(_pickupSoundFraction));
}
