/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand giving what it holds to a creature: the offer while the hand is over a creature that would take it, the
// press held that gives it (the Give packet), the hand drawn to the creature's hand as it takes it, and the hand letting
// go of it into the creature's hand. Wiki: docs/wiki/hand/grab-hold-and-throw.md#giving-a-held-object.

#define LOCATOR_IMPLEMENTATIONS

#include <utility>

#include "Camera/Camera.h"
#include "Creature/CreatureGiving.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/CreaturePose.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "EngineConfig.h"
#include "GameClock.h"
#include "Hand/HandGrabRules.h"
#include "HandSystem.h"
#include "Input/GamePackets.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{
bool RightHanded()
{
	return Locator::config::has_value() && Locator::config::value().rightHandedHand;
}
} // namespace

std::optional<creature_giving::Verdict> HandSystem::GiveVerdict(entt::entity target, entt::entity held) const noexcept
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* creature = registry.Valid(target) ? registry.TryGet<const Creature>(target) : nullptr;
	if (creature == nullptr || !registry.Valid(held))
	{
		return std::nullopt;
	}
	// openblack has no alliances between players, so only the creature's own player, the local one, may give it things;
	// the creature tests whether it could pick the thing up wherever the thing is, off the map in the hand, and only when
	// nothing else refuses it (its test measures its reach the first time)
	creature_giving::GiveFacts facts {
	    .creatureHasPlayer = creature->owner != PlayerNames::NEUTRAL,
	    .giverHasPlayer = true,
	    .friendly = creature->owner == PlayerNames::PLAYER_ONE,
	    .spellSeed = registry.AllOf<SpellSeed>(held),
	    .creatureCanPickUp = true,
	};
	if (creature_giving::MayGive(facts) == creature_giving::Verdict::No)
	{
		return creature_giving::Verdict::No;
	}
	facts.creatureCanPickUp = Locator::creatureObjectActionSystem::has_value() &&
	                          Locator::creatureObjectActionSystem::value().CanPickUpIgnoringMap(target, held);
	return creature_giving::MayGive(facts);
}

bool HandSystem::Give(std::optional<entt::entity> under) noexcept
{
	if (!_held || !Interactable(*_held) || IsHoldingSeed())
	{
		return false;
	}
	const auto held = *_held;
	const bool offeredStillThere = _giving.offeredTo != entt::null && ecs::IsAvailable(_giving.offeredTo);
	const auto creature = creature_giving::GiveTarget(_giving, offeredStillThere, under);
	if (!creature.has_value() || *creature == held || !ecs::IsAvailable(*creature) ||
	    GiveVerdict(*creature, held) != creature_giving::Verdict::Give)
	{
		return false;
	}
	// The frame decides, the turn gives: the creature turns to the camera, at its point on the ground, and reaches for it
	// with its left hand for the player's left hand
	const auto eye = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : glm::vec3(0.0f);
	game_packets::Packet packet {game_packets::Type::Give, *creature};
	packet.value = static_cast<int32_t>(entt::to_integral(held));
	packet.data[0] = RightHanded() ? 0.0f : 1.0f;
	packet.data[1] = eye.x;
	packet.data[2] = eye.z;
	game_packets::Push(packet);
	creature_giving::Given(_giving, *creature);
	_giving.sentTurn = game_clock::Turn();
	// The hand keeps hold of it, no longer ready to throw: the release neither puts it down nor throws it
	_releaseArmed = false;
	_springActive = false;
	return true;
}

void HandSystem::UpdateGiving(bool actionHeld) noexcept
{
	// Only a thing the hand holds of its own, not a seed nor a tree it pulls at, is offered; anything else and all is as
	// before it held anything
	if (!_held || !Interactable(*_held) || IsHoldingSeed() || _tug)
	{
		_giving.Clear();
		_givingHeld = entt::null;
		return;
	}
	if (_givingHeld != *_held)
	{
		_giving.Clear();
		_givingHeld = *_held;
	}
	// The button let go, the press gives nothing
	if (!actionHeld)
	{
		_giving.givePress = false;
	}
	const auto held = *_held;
	const auto under = _cursorObject && *_cursorObject != held && ecs::IsAvailable(*_cursorObject)
	                       ? _cursorObject
	                       : std::optional<entt::entity> {};
	// The button held down a while gives what the hand holds to the creature it offers it to, or to one under the hand
	// that would take it
	if (_giving.givePress && hand_grab::ElapsedMs(game_clock::EngineFrameSampleMs(), _giving.pressMs, game_clock::Turn(),
	                                              _giving.pressTurn) > hand_grab::k_GrabWaitMs)
	{
		_giving.givePress = false;
		static_cast<void>(Give(under));
	}
	// Until the turn after the giving was sent the creature has not been told; from then, while it turns to the player or
	// reaches for it
	const bool notYetTold = _giving.sentTurn.has_value() && game_clock::Turn() <= *_giving.sentTurn;
	const bool stillTaking =
	    _giving.Reaching() && (notYetTold || (Locator::creatureMindSystem::has_value() &&
	                                          Locator::creatureMindSystem::value().IsTakingFromHand(_giving.givenTo)));
	const bool cantPickUp =
	    creature_giving::KeepOffer(_giving, {
	                                            .stillTaking = stillTaking,
	                                            .under = under,
	                                            .verdict = under.has_value() ? GiveVerdict(*under, held) : std::nullopt,
	                                            .frameMs = static_cast<int32_t>(game_clock::FrameRealMs()),
	                                            .hold = _holdType,
	                                            .rightHanded = RightHanded(),
	                                        });
	if (cantPickUp)
	{
		// TODO(creature-giving): the creature's help says it couldn't pick up what it is offered (a stacked
		// miscellaneous message, creature_giving::k_CantPickUpHelp); openblack has no creature help messages yet
	}
}

glm::vec3 HandSystem::GivingTarget(glm::vec3 target, float hang) noexcept
{
	if (!_held || !_giving.Reaching())
	{
		return target;
	}
	const auto creature = _giving.givenTo;
	std::optional<glm::vec3> takePoint;
	std::optional<entt::entity> taking;
	float msFromTake = 0.0f;
	if (Locator::creatureObjectActionSystem::has_value())
	{
		const auto& hands = Locator::creatureObjectActionSystem::value();
		takePoint = hands.TakePoint(creature, _giving.lastHand.value_or(target));
		if (const auto reaching = hands.GetTakingFromHand(creature))
		{
			taking = reaching->object;
			msFromTake = reaching->msFromTake;
		}
	}
	const auto reach = creature_giving::ReachFrame(
	    _giving, {
	                 .target = target,
	                 .takePoint = takePoint,
	                 .creatureSize = ecs::creature_pose::DrawnSize(std::as_const(Locator::entitiesRegistry::value()), creature),
	                 .taking = taking,
	                 .msFromTake = msFromTake,
	                 .held = *_held,
	                 .hang = hang,
	             });
	if (reach.calledOff)
	{
		// The creature gives up taking it at the next turn, as a failure
		game_packets::Push({game_packets::Type::GiveCallOff, creature});
	}
	return reach.hand;
}

bool HandSystem::HandOver(entt::entity object) noexcept
{
	if (!_held || *_held != object || IsHoldingSeed())
	{
		return false;
	}
	// It leaves the hand without physics, for the creature's hand to take
	static_cast<void>(RemoveFirstFromHand());
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(object) && registry.AllOf<HandDrawPose>(object))
	{
		registry.Remove<HandDrawPose>(object);
	}
	_giving.Clear();
	_givingHeld = entt::null;
	_releaseArmed = false;
	_springActive = false;
	return true;
}

bool HandSystem::TakeIntoHand(entt::entity object) noexcept
{
	if (!IsHandReadyForObject() || !Interactable(object))
	{
		return false;
	}
	// As the place-in-hand packet's handler puts a thing in the hand
	ApplyPlaceInHand(object);
	return _held == object;
}

bool HandSystem::GiveHeldTo(entt::entity creature) noexcept
{
	return Give(creature);
}

std::optional<entt::entity> HandSystem::GetOfferedCreature() const noexcept
{
	return _held ? _giving.Offered() : std::nullopt;
}

float HandSystem::GetOfferTurn() const noexcept
{
	return _held ? _giving.turn.GetValue() : 0.0f;
}
