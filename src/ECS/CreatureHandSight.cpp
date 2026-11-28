/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureHandSight.h"

#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "Common/GameAngle.h"
#include "Creature/CreatureAttention.h"
#include "Creature/CreatureHandSight.h"
#include "Creature/CreaturePlayerAttention.h"
#include "Creature/PerceivedDesires.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "InfoConstants.h"
#include "Input/LocalPlayer.h"
#include "Locator.h"

using namespace openblack;

namespace
{
/// Whether the creature can see its player's hand: the player's camera looks at the creature, and the creature looks
/// towards the hand, within the parts of a turn the game asks
bool SeesHand(const ecs::Registry& registry, entt::entity creature, const glm::vec3& hand)
{
	const auto& transform = registry.Get<const ecs::components::Transform>(creature);
	const auto& camera = Locator::camera::value();
	if (!creature_attention::IsLookingAt(camera.GetOrigin(), camera.GetFocus(), transform.position))
	{
		return false;
	}
	const auto* animation = registry.TryGet<const ecs::components::CreatureAnimation>(creature);
	const float look =
	    creature_perceived_desires::LookYaw(animation != nullptr ? animation->lookAt : std::nullopt, transform.position,
	                                        ecs::creature_pose::BodyYaw(registry, creature, transform.rotation));
	const auto heading = static_cast<uint16_t>(game_math::PhysicsRadiansToGameAngle(look));
	const auto parts = static_cast<uint32_t>(map_coords::FtoL(360.0f / creature_hand_sight::k_LookDegrees));
	return creature_player_attention::IsDirectionClose(map_coords::FromMetres({transform.position.x, transform.position.z}),
	                                                   map_coords::FromMetres({hand.x, hand.z}), heading, parts);
}
} // namespace

bool ecs::creature_hand_sight::SeePlayerAction(Registry& registry, PlayerNames player, creature_desires::Desire desire)
{
	// only the player at this computer has a camera that looks at its creature
	if (player != creature::LocalPlayer() || !Locator::leashSystem::has_value() || !Locator::handSystem::has_value() ||
	    !Locator::camera::has_value() || !Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto creature = Locator::leashSystem::value().PlayersCreature(player);
	if (!creature.has_value() || !registry.Valid(*creature) ||
	    !registry.AllOf<components::Creature, components::CreatureMindState, components::Transform>(*creature))
	{
		return false;
	}
	using Side = systems::HandSystemInterface::Side;
	const auto hand = Locator::handSystem::value().GetPlayerHandPositions().at(static_cast<size_t>(Side::Left));
	auto& mind = registry.Get<components::CreatureMindState>(*creature);
	if (!hand.has_value() || !mind.desires.has_value() || !SeesHand(registry, *creature, *hand))
	{
		return false;
	}
	const auto& info = Locator::infoConstants::value();
	const auto row = creature::InfoRow(registry.Get<const components::Creature>(*creature).species);
	const auto index = static_cast<size_t>(desire);
	if (row >= info.creature.size() || index >= info.creatureInitialDesire.size())
	{
		return false;
	}
	const auto& species = info.creature.at(row);
	const float divisor =
	    info.creatureInitialDesire.at(index).observedActionIncreaseTimeDivisor * species.observedActionIncreaseTimeScale;
	auto& state = (*mind.desires)[desire];
	state.increaseSeconds = openblack::creature_hand_sight::IncreaseSecondsAfterSeen(
	    state.increaseSeconds, divisor, species.minDesireIncreaseSeconds, species.maxDesireIncreaseSeconds);
	// Debug readout: the creature saw the hand, and the desire's new growing time
	if (const auto log = spdlog::get("game"); log != nullptr)
	{
		log->debug("Creature {} saw its player's hand show desire {}: it now grows by 1 in {:.3f} s",
		           entt::to_integral(*creature), index, state.increaseSeconds);
	}
	return true;
}
