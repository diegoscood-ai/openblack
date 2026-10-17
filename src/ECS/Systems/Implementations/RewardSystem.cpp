/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "RewardSystem.h"

#include <vector>

#include <entt/core/hashed_string.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Audio.h"
#include "Audio/Game/Banks.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/RewardRules.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "Graphics/ArgbColour.h"
#include "Locator.h"
#include "Particles/ParticleDrawFrame.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The closed chest's model, and the thump of one landing, heard alike wherever the camera is
constexpr entt::hashed_string k_ChestMesh = entt::hashed_string("misc/chest0");
constexpr int k_LandingSample = 174;
/// The landing shakes a camera only near the middle of the world's first corner: the game shakes about the world's
/// origin rather than the chest
constexpr glm::vec3 k_ShakeCentre {0.0f};

/// The dust's numbers come from the C runtime's stream, as the original's Random draws them
float DustRandom(float a, float b)
{
	return game_random::crt::Random(a, b);
}

float LandHeight(glm::vec3 point)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt({point.x, point.z}) : 0.0f;
}
} // namespace

entt::entity RewardSystem::Create(glm::vec3 position, RewardObjectInfo type, std::optional<PlayerNames> player,
                                  entt::entity town, bool fromSky)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	// Given on the land it stands at the height asked for; from the sky it falls to the land itself
	const glm::vec3 ground {position.x, LandHeight(position), position.z};
	registry.Assign<Transform>(entity, fromSky ? ground + glm::vec3(0.0f, reward::k_FallFrom, 0.0f) : position, glm::mat3(1.0f),
	                           glm::vec3(1.0f));
	registry.Assign<Mesh>(entity, k_ChestMesh.value(), static_cast<int8_t>(0), static_cast<int8_t>(1));
	auto& chest = registry.Assign<Reward>(entity);
	chest.type = type;
	chest.player = player;
	chest.town = town;
	chest.landingPoint = ground;
	chest.madeAt = position;
	chest.serial = _nextSerial++;
	// Its dust is made with it, and only shown once it has fallen
	chest.dust = reward::MakeDust(DustRandom);
	if (fromSky)
	{
		chest.state = Reward::State::Falling;
		chest.dustMilliseconds = reward::k_DustMilliseconds;
	}
	else
	{
		// Put on the land, it stands in the map's cells at once
		chest.state = Reward::State::Landed;
		registry.Assign<RewardOnLand>(entity);
	}
	return entity;
}

void RewardSystem::Update(float milliseconds)
{
	_dust.clear();
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<Reward, Transform>([milliseconds, &registry](entt::entity entity, Reward& chest, Transform& transform) {
		if (chest.state == Reward::State::Landed)
		{
			// Its dust fades once it has fallen: its time goes down after each frame it was drawn on, as the
			// original's draw counts it down after it draws it; then whether this frame draws it
			if (chest.dustShown)
			{
				chest.dustMilliseconds = reward::DustAfterFrame(chest.dustMilliseconds, milliseconds);
			}
			chest.dustShown = reward::DrawsDust(false, chest.dustMilliseconds, registry.AllOf<RewardOnLand>(entity));
			return;
		}
		// It is placed by its clock as it stood before this frame, which then runs on
		const float height = reward::FallHeight(chest.seconds);
		transform.rotation = affine::AngleY(reward::FallYaw(chest.seconds));
		chest.seconds += milliseconds * 0.001f;
		if (height > 0.0f)
		{
			transform.position = chest.landingPoint + glm::vec3(0.0f, height, 0.0f);
			return;
		}
		// It thumps down on the land, shaking a camera near the world's origin
		transform.position = chest.landingPoint;
		chest.state = Reward::State::Landed;
		chest.goesIntoMap = true;
		// Its dust shows on the frame it lands, and then only once it is in the map's cells
		chest.dustShown = reward::DrawsDust(true, chest.dustMilliseconds, false);
		// The thump: sample 174 of the in-game bank, no owner, 2D, not tracked
		audio::PlayOptions thump;
		thump.sample = {audio::Bank(audio::SfxBank::InGame), k_LandingSample};
		thump.is3D = false;
		thump.track = false;
		audio::PlaySoundEffect(thump);
		if (Locator::explosionSystem::has_value())
		{
			Locator::explosionSystem::value().AddShake(k_ShakeCentre, reward::k_ShakeRadius, reward::k_ShakeStrength,
			                                           reward::k_ShakeSeconds, false);
		}
	});
	// The dust sprites of the chests whose dust shows this frame, for the renderer
	registry.Each<const Reward>([this](const Reward& chest) {
		if (!chest.dustShown)
		{
			return;
		}
		const auto look = reward::LookOfDust(chest.dustMilliseconds);
		for (const auto& puff : chest.dust)
		{
			_dust.push_back({.position = chest.madeAt + puff.offset,
			                 .size = look.size,
			                 .argb = argb_colour::Argb(puff.rgb[0], puff.rgb[1], puff.rgb[2], look.alpha),
			                 .cell = puff.frame});
		}
	});
}

void RewardSystem::ProcessTurn()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> landed;
	registry.Each<Reward>([&landed](entt::entity entity, Reward& chest) {
		if (chest.goesIntoMap)
		{
			chest.goesIntoMap = false;
			landed.push_back(entity);
		}
	});
	// A chest that landed goes into the map's cells the turn after
	for (const auto entity : landed)
	{
		registry.AssignOrReplace<RewardOnLand>(entity);
	}
}

void RewardSystem::CollectDrawFrame([[maybe_unused]] particles::draw::Frame& frame) const
{
	// The chests' dust is drawn with the other smoke sprites, in the renderer's sprite list, from each chest's
	// dustShown
}
