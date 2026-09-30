/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MagicFireBall.h"

#include <algorithm>

#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/SpellSeed.h"
#include "PSys/PSysManager.h"

using namespace openblack;
using namespace openblack::magic;
using ecs::components::MagicFireBall;
using ecs::components::Transform;

namespace
{
/// g_game +0x205C9C: the fireballs, newest first
std::vector<entt::entity> g_FireBalls;
uint32_t g_Turn = 0;

const GMagicFireBallInfo& InfoOf(const MagicFireBall& ball)
{
	return Locator::infoConstants::value().magicFireBall.at(static_cast<size_t>(ball.infoRow));
}

void Destroy(entt::entity fireball)
{
	auto& registry = Locator::entitiesRegistry::value();
	std::erase(g_FireBalls, fireball);
	if (auto* fire = ecs::fire::Find(fireball))
	{
		ecs::fire::ToBeDeleted(*fire);
	}
	if (registry.Valid(fireball))
	{
		registry.Destroy(fireball);
	}
}
} // namespace

entt::entity fireball::Create(const glm::vec3& position, int infoRow, uint32_t effect, uint32_t atomKey, bool hasPlayer,
                              PlayerNames player, bool scriptCast, entt::entity source)
{
	auto& registry = Locator::entitiesRegistry::value();
	// fn_00682B70: Object(pos, info) (the scale comes with the first FollowAtom), the head of the fireball list
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	auto& ball = registry.Assign<MagicFireBall>(entity);
	ball.infoRow = std::clamp(infoRow, 0, 2);
	ball.effect = effect;
	ball.atomKey = atomKey;
	ball.hasPlayer = hasPlayer;
	ball.player = player;
	ball.seen = true;
	g_FireBalls.insert(g_FireBalls.begin(), entity);
	// SetTemperature(mgr.Strength (+0x54) x info.initialTemperature, the spell's creator): the new fire takes the
	// ball's player (MagicFireBall::GetPlayer 0x682BF0: its manager's)
	const float temperature = Strength(entity) * InfoOf(ball).initialTemperature;
	if (auto* fire = ecs::fire::Create(entity, hasPlayer, player, source); fire != nullptr && fire->temperature < temperature)
	{
		fire->temperature = temperature;
	}
	// +0x58: rained on unless a script cast it
	registry.Get<MagicFireBall>(entity).affectedByRain = !scriptCast;
	if (ecs::fire::TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fireball: ball {} (row {}) at ({:.1f}, {:.1f}, {:.1f}) T {:.0f}",
		                   static_cast<int>(entity), infoRow, position.x, position.y, position.z, ecs::fire::GetTemperature(entity));
	}
	return entity;
}

bool fireball::FollowAtom(entt::entity fireball, const glm::vec3& position, float scale, uint32_t turn)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* transform = registry.TryGet<Transform>(fireball);
	auto* ball = registry.TryGet<MagicFireBall>(fireball);
	if (transform == nullptr || ball == nullptr)
	{
		return false;
	}
	// obj.pos = the atom's MapCoords, obj +0x1C = atom.y - land (the fire reads both through the transform)
	transform->position = position;
	transform->scale = glm::vec3(scale);
	static_cast<void>(turn);
	ball->seen = true;
	return ecs::fire::GetTemperature(fireball) < InfoOf(*ball).deletionTemperature;
}

float fireball::Strength(entt::entity fireball)
{
	const auto* ball = Locator::entitiesRegistry::value().TryGet<const MagicFireBall>(fireball);
	if (ball == nullptr)
	{
		return 1.0f;
	}
	const auto* effect = psys::manager::Find(ball->effect);
	return effect != nullptr ? effect->GetProcessInfo().power : 1.0f;
}

void fireball::ToBeDeleted(entt::entity fireball)
{
	Destroy(fireball);
}

bool fireball::ValidForPlaceInHand(entt::entity fireball, PlayerNames player)
{
	const auto* ball = Locator::entitiesRegistry::value().TryGet<const MagicFireBall>(fireball);
	return ball == nullptr || !ball->hasPlayer || ball->player != player;
}

entt::entity fireball::Catch(entt::entity fireball, PlayerNames player)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::handSystem::has_value() || !registry.Valid(fireball))
	{
		return entt::null;
	}
	auto& hand = Locator::handSystem::value();
	// GInterfaceStatus::IsHandReadyForObject 0x5DC890 (inf: nothing held) and ValidForPlaceInHand
	if (hand.GetHeldObject().has_value() || !ValidForPlaceInHand(fireball, player))
	{
		return entt::null;
	}
	const auto position = registry.Get<const Transform>(fireball).position;
	const auto entity = seed::Create(position, SpellSeedType::Fire, player, -1, 1.0f);
	if (entity == entt::null)
	{
		return entt::null;
	}
	hand.PlaceObjectInMagicHand(entity);
	seed::InterfaceSetInMagicHand(entity);
	auto& component = registry.Get<ecs::components::SpellSeed>(entity);
	seed::SetInactive(component, false);
	seed::AddToChantStore(component, seed::GetChantNeeded(component, component.powerUp));
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fireball: ball {} caught -> seed {} with {:.0f} chants",
	                   static_cast<int>(fireball), static_cast<int>(entity), component.chantStore);
	Destroy(fireball);
	return entity;
}

void fireball::DeleteAndPutIntoSpellSeed(entt::entity fireball, entt::entity seed)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* ball = registry.TryGet<const MagicFireBall>(fireball);
	auto* component = registry.TryGet<ecs::components::SpellSeed>(seed);
	if (ball == nullptr || component == nullptr)
	{
		return;
	}
	component->psysPower *= 1.0f + InfoOf(*ball).catchIncreaseFactor;
	Destroy(fireball);
}

const std::vector<entt::entity>& fireball::All()
{
	return g_FireBalls;
}

void fireball::ProcessTurn(uint32_t turn)
{
	g_Turn = turn;
	auto& registry = Locator::entitiesRegistry::value();
	const auto balls = g_FireBalls;
	for (const auto fireball : balls)
	{
		const auto* ball = registry.TryGet<const MagicFireBall>(fireball);
		// the atom data's dtor 0x682FA0: its atom (or the whole effect) is gone when the rule stopped refreshing it
		if (ball == nullptr || psys::manager::Find(ball->effect) == nullptr || !ball->seen)
		{
			Destroy(fireball);
			continue;
		}
		registry.Get<MagicFireBall>(fireball).seen = false;
	}
}

void fireball::Clear()
{
	g_FireBalls.clear();
}
