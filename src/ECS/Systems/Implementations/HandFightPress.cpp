/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand directing the fight of the player's creature: the interface's fight state, while the player's camera watches
// the creature's duel and the hand is idle and empty. A press of either button goes to the fight: on the player's
// creature or its opponent a move on its body, held until the button is let go to charge the blow; on nothing, a step
// on the arena's ground. Each is a packet the next turn applies. Wiki: docs/wiki/creature/fights.md#the-players-presses.

#define LOCATOR_IMPLEMENTATIONS

#include <utility>

#include <glm/vec2.hpp>

#include "3D/MapCoords.h"
#include "Audio/Audio.h"
#include "ECS/AudioQueries.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Transform.h"
#include "ECS/HandFight.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "GameClock.h"
#include "HandSystem.h"
#include "Input/GamePackets.h"
#include "Input/InterfaceActive.h"
#include "Locator.h"
#include "Magic/Gestures/PowerUpSystem.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{
/// The sample a fight's press plays where it lands, as a 2D one-shot of the game's own sounds
constexpr int k_FightPressSample = 1;

void PlayPressSound(const map_coords::MapCoords& coords)
{
	audio::tags::CreateAtMapCoords(coords, k_FightPressSample, false, 3, 0, false, false, audio::SfxBank::InGame, 0);
}

/// The local player's creature, while there is one
std::optional<entt::entity> LocalCreature()
{
	if (!Locator::playerSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& players = Locator::playerSystem::value();
	return players.GetPrimaryCreature(players.GetLocalPlayer());
}
} // namespace

bool HandSystem::InFightState() const noexcept
{
	return ActionStateNumber() == ecs::hand_fight::k_FightState && !_gripPoint.has_value();
}

void HandSystem::FightPress(ecs::hand_fight::Button button) noexcept
{
	namespace hand_fight = ecs::hand_fight;
	// every press in the fight wipes the gesture's path
	magic::gestures::ClearBuffer();
	if (!Locator::creatureFightSystem::has_value())
	{
		return;
	}
	auto& fights = Locator::creatureFightSystem::value();
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto self = LocalCreature();
	// On a thing the interface keeps: only the player's creature or its opponent takes the press, where the line through
	// the cursor meets its skin, while fewer than twelve of the player's creature's presses show their marks. Anything
	// else under the hand takes the press for nothing
	if (_collideObject.has_value())
	{
		const auto target = *_collideObject;
		if (!self.has_value() || !registry.Valid(target) || !registry.AllOf<Creature, Transform>(target))
		{
			return;
		}
		const auto opponent = fights.OpponentOf(*self);
		const auto body = target == *self                               ? hand_fight::Body::Own
		                  : opponent.has_value() && target == *opponent ? hand_fight::Body::Opponent
		                                                                : hand_fight::Body::Other;
		if (body == hand_fight::Body::Other || fights.MarkCount(*self) >= hand_fight::k_MostWaiting)
		{
			return;
		}
		const auto hit = fights.BodyHit(target, _cursorRayOrigin, _cursorRayDirection);
		if (!hit.has_value())
		{
			return;
		}
		const auto move = hand_fight::BodyMove(body, hit->heightShare);
		// one press's packet at a time: none while an apply waits for its turn
		if (!move.has_value() || _applySentTurn != 0)
		{
			return;
		}
		// the press's mark on the skin, held with it, then its packet
		fights.MarkOnSkin(*self, body == hand_fight::Body::Own, hit->spot);
		game_packets::Push({.type = hand_fight::BodyPacket(button), .object = target, .value = static_cast<int32_t>(*move)});
		_fightPress = FightPressHeld {.button = button, .visualMs = game_clock::VisualMs(), .turn = game_clock::Turn()};
		PlayPressSound(map_coords::FromWorld(registry.Get<const Transform>(target).position));
		return;
	}
	// On the ground: the action's point, while no apply waits for its turn and the player's creature fights
	if (_applySentTurn != 0 || !self.has_value())
	{
		return;
	}
	const auto point = _interactionPoint.value_or(glm::vec3(0.0f));
	const auto coords = map_coords::FromWorld(point);
	if (!map_coords::InBounds(coords) || !fights.IsDuelling(*self))
	{
		FailApply(point);
		return;
	}
	// only within the arena, while fewer than twelve presses show their marks
	const auto arena = fights.ArenaOf(*self);
	const glm::vec2 metres {map_coords::ToMetres(coords.x), map_coords::ToMetres(coords.z)};
	if (!arena.has_value() || !hand_fight::InsideArena(arena->centre, arena->radius, metres) ||
	    fights.MarkCount(*self) >= hand_fight::k_MostWaiting)
	{
		return;
	}
	// the press's mark on the ground, at the point as the press's map position gives it
	fights.MarkOnGround(*self, map_coords::ToWorld(coords));
	game_packets::Packet packet {.type = hand_fight::GroundPacket(button), .position = point};
	packet.coords = coords;
	game_packets::Push(packet);
	_applySentTurn = game_clock::Turn();
	PlayPressSound(coords);
}

void HandSystem::FightRelease() noexcept
{
	if (!_fightPress.has_value())
	{
		return;
	}
	const auto pressed = *std::exchange(_fightPress, std::nullopt);
	// its mark is no longer held
	if (const auto self = LocalCreature(); self.has_value() && Locator::creatureFightSystem::has_value())
	{
		Locator::creatureFightSystem::value().LetGoOfMark(*self);
	}
	const auto visualMs = static_cast<int32_t>(game_clock::VisualMs() - pressed.visualMs);
	const auto held = ecs::hand_fight::HeldMs(visualMs, game_clock::Turn() - pressed.turn);
	game_packets::Push({.type = game_packets::Type::FightCharge, .value = held});
}
