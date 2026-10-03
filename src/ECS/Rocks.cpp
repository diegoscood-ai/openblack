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
#include "Audio/Audio.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/GUtilsAngle.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Fire/FireEffect.h"
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
	return object::Get2DRadius(entity);
}

float Rocks::Height(entt::entity entity)
{
	return object::GetHeight(entity);
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
	// 0x6E74B8..0x6E751D, after SplitInTwo: GAudio::PlaySoundEffect 0x429E30 with bank InGame (GAudio+0x3AC), sample 130
	// G_RockTap_01 + the counter [0xD559AC] (0..3 in turn), owner the rock (+0x20), is3D 1, track 0, at the interface
	// status' +0xC8 (the hand's point)
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), 130 + audio::NextCounter(audio::Counter::RockTap)};
	options.owner = audio::Owner::Thing(entity);
	options.is3D = true;
	options.track = false;
	options.position = handPosition;
	audio::PlaySoundEffect(options);
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
	// 0x6E75B1: o = GetPosFromAngle(a, R2D x 0.7935); 0x6E76A9 / 0x6E76CE: the halves at pos + o and pos - o
	// (MapCoords::operator+ 0x605520 / operator- 0x6055C0 on this +0x14, `lea edi, [esi + 0x14]` 0x6E76A3): both keep
	// the rock's altitude (o's is 0), so each half is the ground at its own point plus that altitude (GetLHPoint)
	const auto o = gutils::GetPosFromAngle(a, offset);
	const auto centre = map_coords::FromWorld(transform.position);
	std::array<entt::entity, 2> halves {};
	for (size_t i = 0; i < halves.size(); ++i)
	{
		const auto at = map_coords::ToWorld(i == 0 ? centre + o : centre - o);
		halves.at(i) = archetypes::MobileStaticArchetype::Create(at, type, 0.0f, 0.0f, yAngle, 0.0f, scale);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Rock: split type {} scale {:.2f} -> 2 x {:.2f}", static_cast<int>(type),
	                   transform.scale.x, scale);
	// Rock::SplitInTwo: the fire passes on to both halves (fn_007308F0, ECS/Fire), then the old rock goes
	for (const auto half : halves)
	{
		fire::CopyFire(entity, half);
	}
	physics::PhysicsObjects::RemoveObject(entity);
	ecs::map_cells::RemoveMapObject(entity); // CleanupWhenDeleted 0x6377F0: RemoveMapObject vt +0x548
	registry.Destroy(entity);
	registry.SetDirty();
	// Object::InitialisePhysics (vt 0x784): they fall and settle, or fly on
	for (const auto half : halves)
	{
		if (auto* po = physics::PhysicsObjects::AddObject(half, velocity, glm::vec3(0.0f)))
		{
			po->body.angularMomentum = angularMomentum;
		}
	}
	return halves;
}
