/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Rocks.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/Sound.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// info.dat GMobileStaticInfo::mobileType of the Rock class
constexpr int k_RockMobileType = 2;

/// Mesh box size x scale, zero without a loaded mesh.
glm::vec3 ScaledSize(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mesh = registry.TryGet<const Mesh>(entity);
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (mesh == nullptr || !meshes.Contains(mesh->id))
	{
		return glm::vec3(0.0f);
	}
	return meshes.Handle(mesh->id)->GetBoundingBox().Size() * registry.Get<const Transform>(entity).scale;
}
} // namespace

bool Rocks::IsRock(entt::entity entity)
{
	const auto* statics = Locator::entitiesRegistry::value().TryGet<const MobileStatic>(entity);
	if (statics == nullptr || !Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& info = Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(statics->type));
	return static_cast<int>(info.mobileType) == k_RockMobileType;
}

float Rocks::Radius2D(entt::entity entity)
{
	const auto size = ScaledSize(entity);
	return 0.5f * std::max(size.x, size.z);
}

float Rocks::Height(entt::entity entity)
{
	return ScaledSize(entity).y;
}

bool Rocks::ValidForPlaceInHand(entt::entity entity)
{
	return Radius2D(entity) <= 3.6f;
}

bool Rocks::ValidToTap(entt::entity entity)
{
	return Height(entity) > 0.7f;
}

std::array<entt::entity, 2> Rocks::Tap(entt::entity entity, glm::vec3 handPosition)
{
	const auto halves = SplitInTwo(entity);
	// LH_SAMPLE_G_ROCKTAP_01 + i, i turning 0..3 (static 0xD559AC), played at the hand
	static int s_Next = 0;
	constexpr std::array<audio::SoundId, 4> k_Samples = {audio::SoundId::G_RockTap_01_1, audio::SoundId::G_RockTap_02_1,
	                                                      audio::SoundId::G_RockTap_03_1, audio::SoundId::G_RockTap_04_1};
	const auto soundId = static_cast<entt::id_type>(k_Samples.at(s_Next));
	s_Next = (s_Next + 1) % 4;
	if (Locator::audio::has_value() && Locator::resources::value().GetSounds().Contains(soundId))
	{
		Locator::audio::value().PlaySound(soundId, audio::PlayType::Once);
	}
	// TODO: GPlayer::MakeCreatureEmpathiseWithPlayer(CREATURE_DESIRE_TO_PLAY, 0.5, Pos) once creatures have desires
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Rock: tapped at ({:.1f}, {:.1f}, {:.1f})", handPosition.x, handPosition.y,
	                   handPosition.z);
	return halves;
}

std::array<entt::entity, 2> Rocks::SplitInTwo(entt::entity entity, glm::vec3 velocity, glm::vec3 angularMomentum)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<const Transform>(entity);
	const auto type = registry.Get<const MobileStatic>(entity).type;
	constexpr float k_Factor = 0.7935f; // 0x942110, about the cube root of 1/2: each half has half the volume
	const float scale = transform.scale.x * k_Factor;
	const float offset = Radius2D(entity) * k_Factor;
	// Only the Y angle is kept. MobileStaticArchetype's rotation has column 2 = (-sin y cos x, sin x, cos y cos x).
	const float yAngle = std::atan2(-transform.rotation[2].x, transform.rotation[2].z);
	const float a = Locator::rng::value().NextValue(0.0f, glm::two_pi<float>());
	const glm::vec2 direction(std::cos(a), std::sin(a));
	const glm::vec2 centre(transform.position.x, transform.position.z);
	std::array<entt::entity, 2> halves {};
	for (size_t i = 0; i < halves.size(); ++i)
	{
		const auto at = centre + (i == 0 ? offset : -offset) * direction;
		const float ground = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(at) : 0.0f;
		halves.at(i) = archetypes::MobileStaticArchetype::Create(glm::vec3(at.x, ground, at.y), type, 0.0f, 0.0f, yAngle,
		                                                         0.0f, scale);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Rock: split type {} scale {:.2f} -> 2 x {:.2f}", static_cast<int>(type),
	                   transform.scale.x, scale);
	physics::PhysicsObjects::RemoveObject(entity);
	registry.Destroy(entity);
	registry.SetDirty();
	// Object::InitialisePhysics (vt 0x784): they fall and settle, or fly on. TODO: the fire passes on (fn_007308F0)
	for (const auto half : halves)
	{
		if (auto* po = physics::PhysicsObjects::AddObject(half, velocity, glm::vec3(0.0f)))
		{
			po->body.angularMomentum = angularMomentum;
		}
	}
	return halves;
}
