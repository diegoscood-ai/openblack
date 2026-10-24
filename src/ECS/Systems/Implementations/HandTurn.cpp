/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's packets (sent by the interface, applied at the start of the next turn) and the hand's per-turn pass.

#define LOCATOR_IMPLEMENTATIONS

#include <tuple>
#include <utility>

#include <glm/gtc/constants.hpp>

#include "3D/LandMorph.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Services/Guidance.h"
#include "Camera/Camera.h"
#include "Creature/CreatureDeeds.h"
#include "Creature/CreatureHandRules.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureTurnPose.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureTurnPose.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/CreatureHandPackets.h"
#include "ECS/CreatureMimic.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/HandActionPoint.h"
#include "ECS/HandCreature.h"
#include "ECS/HandValidate.h"
#include "ECS/Influence/Influence.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/ObjectResources.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/Physics/ParticleCarriedObjects.h"
#include "ECS/PlayerAlliances.h"
#include "ECS/PuzzleTotemHand.h"
#include "ECS/Registry.h"
#include "ECS/Scaffolds.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandTap.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "GameClock.h"
#include "Hand/HandGrabRules.h"
#include "HandSystem.h"
#include "HandSystemDetail.h"
#include "Help/HelpProfile.h"
#include "Help/HelpSystem.h"
#include "Input/GamePackets.h"
#include "Input/HandDemo.h"
#include "Input/InterfaceActive.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Magic/HandHoldPose.h"
#include "Magic/Objects/MagicFireBall.h"
#include "Particles/ParticleManager.h"
#include "Worship/InterfaceStatus.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The player a creature belongs to, through the const registry; nobody for anything else
PlayerNames CreatureOwnerForHand(entt::entity object)
{
	const auto* creature = std::as_const(Locator::entitiesRegistry::value()).TryGet<const Creature>(object);
	return creature != nullptr ? creature->owner : PlayerNames::NEUTRAL;
}
} // namespace

HandSystem::~HandSystem()
{
	game_packets::Reset();
	game_packets::ClearHandlers();
}

void HandSystem::RegisterPacketHandlers() noexcept
{
	using game_packets::Packet;
	using game_packets::Type;
	// Tap: the object still interactable and valid to tap, then it is tapped
	game_packets::SetHandler(Type::Tap, [this](const Packet& packet) { ApplyTap(packet.object); });
	// The synced MapCoords and hand, the synced camera, and the ping of the local interface (ReceivePing). (not ported)
	// a few copies of the sync fields that are only saved (no reader found)
	game_packets::SetHandler(Type::HandAndCamera, [this](const Packet& packet) {
		ReceivePing(packet.value);
		_syncMapCoords = packet.coords;
		_syncHand = glm::vec3(packet.data[0], packet.data[1], packet.data[2]);
		_syncCameraPosition = glm::vec3(packet.data[3], packet.data[4], packet.data[5]);
		_syncCameraFocus = glm::vec3(packet.data[6], packet.data[7], packet.data[8]);
	});
	game_packets::SetHandler(Type::Hand, [this](const Packet& packet) {
		ReceivePing(packet.value);
		_syncMapCoords = packet.coords;
		_syncHand = glm::vec3(packet.data[0], packet.data[1], packet.data[2]);
	});
	game_packets::SetHandler(Type::Camera, [this](const Packet& packet) {
		ReceivePing(packet.value);
		_syncCameraPosition = glm::vec3(packet.data[3], packet.data[4], packet.data[5]);
		_syncCameraFocus = glm::vec3(packet.data[6], packet.data[7], packet.data[8]);
	});
	// The throw data: velocity, angular velocity, position and YXZ angles: the throw that the next apply-to-map-coord,
	// throw-held or apply-to-object packet uses
	game_packets::SetHandler(Type::ThrowData, [this](const Packet& packet) {
		_statusThrowVelocity = glm::vec3(packet.data[0], packet.data[1], packet.data[2]);
		_statusThrowAngular = glm::vec3(packet.data[3], packet.data[4], packet.data[5]);
		_statusThrowHandPosition = glm::vec3(packet.data[6], packet.data[7], packet.data[8]);
		_statusThrowAngles = glm::vec3(packet.data[9], packet.data[10], packet.data[11]);
	});
	// Apply to a map coord: an empty hand ends the action; the held object must be available and accept a map coord
	// (every holdable class but the seed does), else nothing; then it is applied at the packet's MapCoords (the throw
	// with the throw data) and the result is handled. (not identified) one more call on the status
	game_packets::SetHandler(Type::ApplyToMapCoord, [this](const Packet& packet) {
		_applyHandledTurn = game_clock::Turn();
		if (!_held)
		{
			EndAction();
			return;
		}
		// what the hand keeps of its player's influence past the border is brought up to the hand's synced place first
		ProcessVirtualInfluence();
		if (!ecs::IsAvailable(*_held))
		{
			return;
		}
		if (IsHoldingSeed())
		{
			ApplySeedToMapCoord(packet);
			return;
		}
		const auto held = *_held;
		HandleApplyResult(Release(_statusThrowVelocity, packet.position, true), held, packet.position);
	});
	// Apply unlock, the end of a locked apply (sent after its applies): the turns it was held are kept first; then the
	// packet's object, if the hand can still use it, is unlocked, which for a seed gives its spell's chants back and lets
	// the spell close (a seed used up goes; any other object does nothing); no such object ends the action
	game_packets::SetHandler(Type::ApplyUnlock, [this](const Packet& packet) {
		_unlockTurnsHeld = packet.value;
		if (packet.object == entt::null || !Interactable(packet.object))
		{
			EndAction();
			return;
		}
		auto& registry = Locator::entitiesRegistry::value();
		if (!registry.AllOf<SpellSeed>(packet.object))
		{
			return;
		}
		const auto seed = packet.object;
		magic::seed::ApplyUnlockProcess(seed);
		if (!registry.Valid(seed) && _held == seed)
		{
			SeedLeftHand(seed);
			_held.reset();
		}
	});
	// Place in hand: the checks again, then the object goes into the hand; a refusal or no object ends the action
	game_packets::SetHandler(Type::PlaceInHand, [this](const Packet& packet) { ApplyPlaceInHand(packet.object); });
	// Apply to object: the held object is applied to the target (a seed's: HandSpellSeed.cpp); no target ends the
	// action
	game_packets::SetHandler(Type::ApplyToObject, [this](const Packet& packet) {
		_applyHandledTurn = game_clock::Turn();
		if (packet.object == entt::null)
		{
			EndAction();
		}
		else if (IsHoldingSeed())
		{
			ApplySeedToObject(packet);
		}
		else
		{
			ApplyHeldToObject(packet.object);
		}
	});
	// The locked select's start and end
	game_packets::SetHandler(Type::StartLockedSelect, [this](const Packet& packet) { ApplyStartLockedSelect(packet.object); });
	game_packets::SetHandler(Type::EndLockedSelect, [this](const Packet& packet) { ApplyEndLockedSelect(packet.object); });
	// The altitude the hand holds a puzzle totem at, sent at each new step of its drag and at its end: the totem's height
	// follows it (no totem: nothing)
	game_packets::SetHandler(Type::PuzzleTotemPosition, [](const Packet& packet) {
		if (packet.object != entt::null)
		{
			ecs::puzzle_totem_hand::ApplyPosition(packet.object, packet.coords.altitude);
		}
	});
	// The share a village totem was let go at, sent at its locked select's end: the totem takes it (no totem: nothing)
	game_packets::SetHandler(Type::TotemShare, [](const Packet& packet) {
		if (packet.object != entt::null && Locator::villageTotemSystem::has_value())
		{
			Locator::villageTotemSystem::value().LetGo(packet.object, packet.data[0]);
		}
	});
	// The creature the hand let go of: how it was treated reaches its mind; and a click on the player's own creature
	// works their leash key
	game_packets::SetHandler(Type::CreatureFeedback, [](const Packet& packet) {
		ecs::creature_hand_packets::ApplyFeedback(
		    Locator::creatureMindSystem::has_value() ? &Locator::creatureMindSystem::value() : nullptr, packet);
	});
	// A hard slap that bruised: the sender's own creature loses a little life
	game_packets::SetHandler(Type::CreatureHardSlap, [](const Packet& packet) {
		ecs::creature_hand_packets::ApplyHardSlap(Locator::playerSystem::has_value()
		                                              ? Locator::playerSystem::value().GetPrimaryCreature(packet.player)
		                                              : std::nullopt);
	});
	game_packets::SetHandler(Type::CreatureLeashClick, [](const Packet&) {
		ecs::creature_hand_packets::ApplyLeashClick(
		    Locator::leashSystem::has_value() ? &Locator::leashSystem::value() : nullptr, PlayerNames::PLAYER_ONE,
		    Locator::magicSystem::has_value() ? &magic::gestures::State().leashSelection : nullptr);
	});
	// Give: what the hand still holds goes to the creature, which turns to the camera and reaches for it; and the giving
	// called off when the hand went too far from the creature reaching for it
	game_packets::SetHandler(Type::Give, [this](const Packet& packet) {
		if (_held && static_cast<uint32_t>(packet.value) == entt::to_integral(*_held))
		{
			std::ignore = ecs::creature_hand_packets::ApplyGive(
			    Locator::creatureMindSystem::has_value() ? &Locator::creatureMindSystem::value() : nullptr, packet);
		}
	});
	game_packets::SetHandler(Type::GiveCallOff, [](const Packet& packet) {
		ecs::creature_hand_packets::ApplyGiveCallOff(
		    Locator::creatureMindSystem::has_value() ? &Locator::creatureMindSystem::value() : nullptr,
		    Locator::creatureObjectActionSystem::has_value() ? &Locator::creatureObjectActionSystem::value() : nullptr, packet);
	});
	// Throw held: the held object is thrown from the hand with the throw data's zero velocity
	game_packets::SetHandler(Type::ThrowHeld, [this](const Packet&) { ApplyForceDropHeld(); });
	// A particle spot visual at the position
	game_packets::SetHandler(Type::SpotVisual, [](const Packet& packet) {
		Locator::particleSystem::value().StartSpotVisual(static_cast<SpotVisualType>(packet.value), packet.position,
		                                                 std::nullopt, entt::null);
	});
	// Release impulse: the released object's body (none: nothing) spins with the hand's move after the release (the
	// spring velocity is not read)
	game_packets::SetHandler(Type::ReleaseImpulse, [](const Packet& packet) {
		ecs::physics::from_hand::ApplyReleaseSpin(packet.object, glm::vec3(packet.data[0], packet.data[1], packet.data[2]));
	});
}

bool HandSystem::Interactable(entt::entity object) noexcept
{
	// Available; an abode also needs to be partly built
	auto& registry = Locator::entitiesRegistry::value();
	const bool available = object != entt::null && ecs::IsAvailable(object);
	const bool abode = available && registry.AllOf<Abode>(object);
	return ecs::hand_action_point::Interactable(available, abode, abode ? abodes::GetPercentBuilt(object) : 1.0f);
}

void HandSystem::PushThrowData(glm::vec3 velocity, bool predict) noexcept
{
	// The throw data: the velocity, the angular momentum, the position and the YXZ angles.
	// - predicting (state 12): from a prediction body set up where the hand drew the held object (L = h M8, the
	//   velocity capped at 124, adjusted to the ground, run for min(ping / 100, 5) turns): its velocity, L, origin and
	//   YXZ angles: from_hand::PredictRelease; no body (an openblack guard): the drawn pose, the capped velocity, L 0
	// - ForceDropHeld (not predicting): the velocity and L zero, the held object's matrix translation and YXZ angles
	game_packets::Packet data {game_packets::Type::ThrowData};
	auto& registry = Locator::entitiesRegistry::value();
	if (!_held || !registry.Valid(*_held))
	{
		if (!predict)
		{
			game_packets::Push(data); // ForceDropHeld's own empty-hand path is in HandSpellSeed.cpp
		}
		return; // state 12: nothing without a held object
	}
	const auto held = *_held;
	// the held object's drawn matrix (its G3D), the frame's: HandDrawPose, else its Transform
	using magic::hand_hold::HeldPlace;
	const auto& transform = registry.Get<const Transform>(held);
	const auto* pose = registry.TryGet<const HandDrawPose>(held);
	const auto drawn = pose != nullptr ? std::optional(HeldPlace {pose->position, pose->rotation}) : std::nullopt;
	const auto start = magic::hand_hold::ReleasePlace({transform.position, transform.rotation}, drawn);
	glm::vec3 angular(0.0f);
	glm::vec3 origin = start.position;
	glm::mat3 rotation = start.rotation;
	if (predict)
	{
		// the hand's angular velocity h (0 while the Holding spring works: an object, not a seed)
		const glm::vec3 handAngular = _handAngularVelocity;
		const auto turns = std::min(_pingMs / 100u, 5u); // unsigned division, signed min
		// the body starts where the hand drew the object, where it hangs from the hand, not at the place the game turn
		// gave it, which trails a moving hand by a turn or more and sits at the hand point
		const auto prediction =
		    ecs::physics::from_hand::PredictRelease(held, velocity, handAngular, turns, start.position, start.rotation);
		if (prediction)
		{
			velocity = prediction->velocity;
			angular = prediction->angularMomentum;
			origin = prediction->origin;
			rotation = prediction->rotation;
		}
		else
		{
			velocity = ecs::physics::from_hand::CapReleaseVelocity(velocity).velocity;
		}
	}
	float y = 0.0f;
	float x = 0.0f;
	float z = 0.0f;
	affine::DecomposeYXZ(rotation, y, x, z);
	data.data = {velocity.x, velocity.y, velocity.z, angular.x, angular.y, angular.z, origin.x, origin.y, origin.z, x, y, z};
	game_packets::Push(data);
}

int32_t HandSystem::NextPingIndex() noexcept
{
	// ring[index] = GetTickCount, the old index into the packet, index + 1 (32 entries). (approximate) the engine
	// timer's ms for GetTickCount (the same wall clock, not its 15.6 ms steps; a restart of the engine timer would wrap
	// the difference, which GetTickCount never does)
	const auto index = _pingIndex;
	_pingRing[index] = static_cast<uint32_t>(game_clock::EngineMs());
	_pingIndex = static_cast<uint8_t>((index + 1) % _pingRing.size());
	return index;
}

void HandSystem::ReceivePing(int32_t index) noexcept
{
	// For the local interface: the ping = GetTickCount - ring[the packet's ping index]; openblack's packets are all the
	// local interface's
	_pingMs = static_cast<uint32_t>(game_clock::EngineMs()) - _pingRing[static_cast<size_t>(index) % _pingRing.size()];
}

void HandSystem::SendRelease(glm::vec3 velocity) noexcept
{
	// On the release (state 12): the action's MapCoords in bounds, an object in the hand that accepts a map coord (every
	// class this sends for) and influence there: the throw data packet. Otherwise no throw data and, in HOLDING, the
	// spring stops. Both go on to the drop on the map coord: nothing while an apply packet waits; the same three
	// tests: the apply packet, or FailApply: the fail spot and sound, the object stays in the hand. A cleared action
	// point is the map's corner, which is in bounds
	const auto point = _interactionPoint.value_or(glm::vec3(0.0f));
	const bool valid = _held && map_coords::InBounds(point) && InInfluence();
	if (valid)
	{
		PushThrowData(velocity, true);
	}
	else
	{
		// HOLDING (hand state 4): the spring stops, then the drop on the map coord; no buffer clear on this path (that is
		// RemoveFirstFromHand's). (approximate) one _springActive for every state (the original's flag is HOLDING's own),
		// which HandPlacement also clears
		if (_renderHandState == 4)
		{
			_springActive = false;
		}
	}
	if (!_held || _applySentTurn != 0)
	{
		return;
	}
	if (!valid)
	{
		FailApply(point);
		return;
	}
	game_packets::Push({game_packets::Type::ApplyToMapCoord, entt::null, point});
	_applySentTurn = game_clock::Turn();
}

void HandSystem::GameTurnUpdate() noexcept
{
	// After the grain hand state's update (MagicLoop.cpp): the hand's object no longer available is thrown, else its
	// hold type is refreshed. (approximate) the hold type is ComputeHoldParameters' (the radius, lowering and height
	// stay those of the pick-up)
	auto& registry = Locator::entitiesRegistry::value();
	if (_renderHandHeld)
	{
		if (!registry.Valid(*_renderHandHeld) || !ecs::IsAvailable(*_renderHandHeld))
		{
			RenderHandRelease();
		}
		else
		{
			const auto radius = _holdRadius;
			const auto lowering = _loweringMultiplier;
			const auto height = _heldHeight;
			const auto rooted = _rooted;
			ComputeHoldParameters(*_renderHandHeld);
			_holdRadius = radius;
			_loweringMultiplier = lowering;
			_heldHeight = height;
			_rooted = rooted;
		}
	}
	// lastReleased no longer available: it and the countdown at 0. (not ported) the creature hand's fields and the
	// spell-in-hand particles
	if (_lastReleased && (!registry.Valid(*_lastReleased) || !ecs::IsAvailable(*_lastReleased)))
	{
		_lastReleased.reset();
		_releaseImpulseMs = 0;
	}
}

void HandSystem::UpdateReleaseImpulse() noexcept
{
	// Each frame, while preparing to draw. Inside the citadel nothing. With an object: HOLDING sets the countdown to 180;
	// the interface's hand state 0x19 (INVISIBLE), a locked pile, a seed (GRAIN) or the tug (TUG) leave it
	if (game_clock::IsInsideCitadel())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto held = RenderHeld(); held)
	{
		const bool pile = _pickSource && ecs::object_resources::IsPileResource(*_pickSource);
		const bool seed = registry.Valid(*held) && registry.AllOf<SpellSeed>(*held);
		if (_interfaceHandState != 0x19 && !pile && !seed && !_tug)
		{
			_releaseImpulseMs = 180;
		}
		return;
	}
	// With nothing held, the countdown goes down by the frame's game ms; the frame it turns negative (exactly 0 stops it
	// with nothing sent) and lastReleased is set, the countdown = 0 and the release impulse packet: lastReleased, the
	// held ground point minus the required hand position, and the Holding spring velocity; then lastReleased is
	// cleared. The ground point is the last normal, camera, holding or grain update's, the required position the last
	// holding or grain frame's
	if (hand_grab::CountDown(_releaseImpulseMs, game_clock::FrameGameMs()) != hand_grab::Countdown::RunOut || !_lastReleased)
	{
		return;
	}
	_releaseImpulseMs = 0;
	const auto delta = _heldGroundPoint - _requiredHandPosition;
	game_packets::Packet packet {game_packets::Type::ReleaseImpulse, *_lastReleased};
	packet.data = {delta.x, delta.y, delta.z, _springVelocity.x, _springVelocity.y, _springVelocity.z};
	game_packets::Push(packet);
	_lastReleased.reset();
}

void HandSystem::SendPlaceInHand(entt::entity entity) noexcept
{
	// Its sounds at once, the place-in-hand packet and the action state WAIT FOR PLACE IN HAND (the press is kept until
	// the object is in the hand)
	// in the physics (InPhysics): a moving body, a carried object or one left hanging where a hand let go of it
	GenericPickupSounds(entity, Locator::entitiesRegistry::value().AllOf<ecs::components::InPhysics>(entity));
	game_packets::Push({game_packets::Type::PlaceInHand, entity});
	_pickPressHeld = true;
}

void HandSystem::ApplyPlaceInHand(entt::entity object) noexcept
{
	// The place-in-hand packet, re-entered from the big forest and the magic teleport (they put another object in the
	// hand)
	auto& registry = Locator::entitiesRegistry::value();
	// an object, interactable, space in the hands, ValidForPlaceInHand, not IsCannotBePickedUp, and not carried by a
	// particle system (particle_carried_objects::IsCarried). (pending) two flags whose names are unconfirmed:
	// UNAVAILABLE_FOR_STATE_CHANGE and LOCKED_SELECT
	if (!Interactable(object) || _held || !ValidForPlaceInHand(object) || ecs::object_flags::IsCannotBePickedUp(object) ||
	    ecs::physics::particle_carried_objects::IsCarried(object))
	{
		EndAction();
		_pickPressHeld = false;
		return;
	}
	// The object goes into the hand. The ones that return 0 put another object in the hand through this function again;
	// the outer call then only ends the action.
	// (pending) the other re-entrant classes (field, fish farm, field crop, pile); the object's fire noticing that it
	// started moving
	if (registry.AllOf<BigForest>(object))
	{
		// BigForest: a resource removed, a tree created and put in the hand, then 0
		if (const auto tree = CreateTreeFromForest(object); tree != entt::null)
		{
			ApplyPlaceInHand(tree);
		}
		EndAction();
		_pickPressHeld = _held.has_value();
		return;
	}
	if (registry.AllOf<TeleportStone>(object))
	{
		// MagicTeleport: the spell's seed put in the hand, then 0
		if (const auto seed = SeedToPlaceInHand(object); seed != entt::null)
		{
			ApplyPlaceInHand(seed);
		}
		EndAction();
		_pickPressHeld = _held.has_value();
		return;
	}
	if (registry.AllOf<MagicFireBall>(object))
	{
		// MagicFireBall: the catch puts a new fire seed in the hand and deletes the ball, then 0 whether it caught or not
		static_cast<void>(magic::fireball::Catch(object, PlayerNames::PLAYER_ONE));
		EndAction();
		_pickPressHeld = _held.has_value();
		return;
	}
	if (registry.AllOf<SpellSeed>(object))
	{
		// SpellSeed (the chants and age stored from the spell, the spell link cleared: its spell closes down): 1 the hand
		// takes it (worship's PlaceSeedInMagicHand does both); 3 ToBeDeleted -> the action ends
		if (worship::interface::PlaceSeedInMagicHand(PlayerNames::PLAYER_ONE, object) != 1)
		{
			EndAction();
			_pickPressHeld = false;
		}
		return;
	}
	// 1: the rest (PickUp). A rooted tree (not flying) leaves the ground here: PickUp plays the tree's break sound and
	// updates its alignment (its roots follow it per frame, HandHolding). (pending) the reaction 0x10; the help
	// profile triggers 2 / 3 in PickUp
	PickUp(object, false);
}

void HandSystem::SendStartLockedSelect(entt::entity object) noexcept
{
	// The object's network-unfriendly start (nothing for most objects; a village totem's: the share the hand holds it at
	// and the raising sound; a puzzle totem's: the drag from where it stands), the start packet and action state 3 with
	// the object as the action's
	ecs::puzzle_totem_hand::StartLockedSelect(object);
	if (std::as_const(Locator::entitiesRegistry::value()).AllOf<VillageTotem>(object) &&
	    Locator::villageTotemSystem::has_value())
	{
		Locator::villageTotemSystem::value().StartLockedSelect(object);
	}
	game_packets::Push({game_packets::Type::StartLockedSelect, object});
	_lockedSelectAction = object;
}

void HandSystem::SendEndLockedSelect() noexcept
{
	// State 3's end: with an action object that is still available, its network-unfriendly end (nothing for piles,
	// fields and fish farms; a village totem's: the share it was held at sent, the raising sound's end) and the end
	// packet; an object that is gone gets neither. Then the multi pickup stops at once
	if (_lockedSelectAction)
	{
		if (const auto object = *_lockedSelectAction; ecs::IsAvailable(object))
		{
			if (std::as_const(Locator::entitiesRegistry::value()).AllOf<VillageTotem>(object) &&
			    Locator::villageTotemSystem::has_value())
			{
				Locator::villageTotemSystem::value().EndLockedSelect(object);
			}
			else
			{
				// a puzzle totem's: the altitude the hand holds it at, sent once more
				ecs::puzzle_totem_hand::EndLockedSelect(object);
			}
			game_packets::Push({game_packets::Type::EndLockedSelect, object});
		}
		_lockedSelectAction.reset();
		_lockedSelectStopped = true;
	}
}

void HandSystem::ApplyStartLockedSelect(entt::entity object) noexcept
{
	// The start packet: an interactable object not in a locked select already ((inferred) openblack has the one hand,
	// so its locked object), then the network-friendly start, the status's locked object and the locked-select flag.
	// Otherwise the action ends (state 3's end: the end packet).
	// (inferred) a hand already holding something refuses it: state 3 begins only on a press with an empty hand
	auto& registry = Locator::entitiesRegistry::value();
	if (_pickSource == object || _held || !Interactable(object))
	{
		SendEndLockedSelect();
		return;
	}
	// past those checks the interface keeps the object as its locked one, whatever the object's own start does
	_statusLockedObject = object;
	if (ecs::puzzle_totem_hand::IsPuzzleTotem(object))
	{
		// a puzzle totem: the hand holds it (no reaction, no creature watching)
		ecs::puzzle_totem_hand::SetHeld(object, true);
		_lockedSelectStopped = false;
		return;
	}
	if (std::as_const(registry).AllOf<VillageTotem>(object) && Locator::villageTotemSystem::has_value())
	{
		// a village totem: its network-friendly start (the living's reaction to the hand using it), then the hand holds
		// it
		auto& totems = Locator::villageTotemSystem::value();
		totems.FriendlyStartLockedSelect(object, PlayerNames::PLAYER_ONE);
		totems.SetGripped(object, true);
		_lockedSelectStopped = false;
		return;
	}
	if (std::as_const(registry).AllOf<Creature>(object))
	{
		// a creature: now locked for the hand, which takes hold of it (the creature hand) when it may. A refused grab
		// still leaves it locked, and the select goes on until the release. Then the creature lets go of the miracle it
		// is casting. (openblack guard) a start for a creature the hand did not press on ends the select. The interface's
		// own creature-lock flag is set whatever the hand can do
		_statusCreatureLocked =
		    hand_creature::StepCreatureLockFlag(_statusCreatureLocked, hand_creature::LockFlagEvent::CreatureStart);
		_creatureLocked = _creatureLock == object;
		// it first finishes what its body is doing: the hand strokes and slaps it only once that is done
		_creatureFinishing = _creatureLocked;
		if (_creatureLocked && Locator::creatureHandSystem::has_value())
		{
			(void)Locator::creatureHandSystem::value().Grab(object);
		}
		if (Locator::magicSystem::has_value())
		{
			Locator::magicSystem::value().ReleaseCreatureCast(object);
		}
		if (!_creatureLocked)
		{
			SendEndLockedSelect();
		}
		return;
	}
	// The network-friendly start: Field, FishFarm, pile (the first amount into a hand pot put in the hand, and the multi
	// pickup starts)
	// the hand pot at the status's MapCoords
	const auto point = SyncMapPoint();
	if (registry.AllOf<Field>(object))
	{
		TryPickUpField(object, point);
	}
	else if (registry.AllOf<FishFarm>(object))
	{
		TryPickUpFish(object, point);
	}
	else if (registry.AllOf<Pot>(object))
	{
		// the pile's player is read before the first scoop, which may empty and delete a loose pile; once the hand pot is
		// in the hand the interface records it
		const auto pilePlayer = ecs::object_resources::PlayerOfPile(object);
		PickUp(object, false, point);
		if (_pickSource == object)
		{
			_sourceOwner = pilePlayer;
		}
	}
	if (!_pickSource)
	{
		// (inferred) nothing was taken: the same turn's status pass finds no hand pot to fill, its interaction returns 0,
		// the locked object is cleared and the select ends (EndAction)
		_statusLockedObject.reset();
		SendEndLockedSelect();
		return;
	}
	_lockedSelectStopped = false;
}

void HandSystem::ApplyEndLockedSelect(entt::entity object) noexcept
{
	// no object (gone): EndAction and nothing else; the turn's pass then ends the select (IsAvailable)
	if (object == entt::null)
	{
		SendEndLockedSelect();
		return;
	}
	// any other end clears the interface's locked object, whichever object it was
	_statusLockedObject.reset();
	if (ecs::puzzle_totem_hand::IsPuzzleTotem(object))
	{
		// a puzzle totem: the hand no longer holds it, and it stands as high as its height says
		ecs::puzzle_totem_hand::SetHeld(object, false);
		return;
	}
	if (std::as_const(Locator::entitiesRegistry::value()).AllOf<VillageTotem>(object) &&
	    Locator::villageTotemSystem::has_value())
	{
		// a village totem: the hand no longer holds it; its network-friendly end does nothing more
		Locator::villageTotemSystem::value().SetGripped(object, false);
		return;
	}
	if (std::as_const(Locator::entitiesRegistry::value()).AllOf<Creature>(object))
	{
		// a creature: no longer locked for the hand, and its body goes back to the game
		_statusCreatureLocked =
		    hand_creature::StepCreatureLockFlag(_statusCreatureLocked, hand_creature::LockFlagEvent::CreatureEnd);
		auto& registry = Locator::entitiesRegistry::value();
		if (auto* pose = registry.TryGet<CreatureTurnPose>(object))
		{
			creature_turn_pose::Reconnect(*pose);
		}
		if (_creatureLock == object)
		{
			_creatureLocked = false;
			_creatureFinishing = false;
		}
		return;
	}
	// The end packet: the object's network-friendly end (a pile: the multi pickup stops and the amount tooltip,
	// in HandSystem::Update), the status's locked object cleared and the locked-select flag cleared
	_pickSource.reset();
	_pickFish = false;
	_pickField = false;
}

bool HandSystem::IsInLockedSelect(entt::entity object) const noexcept
{
	// The select's start marks its thing, and the end applied clears it: the creature locked for the hand, the totem it
	// grips, or the source it takes from
	const bool gripped = (Locator::villageTotemSystem::has_value() && Locator::villageTotemSystem::value().IsGripped(object)) ||
	                     ecs::puzzle_totem_hand::IsHeld(object);
	return hand_creature::IsInLockedSelect({.creature = _creatureLocked ? _creatureLock : std::nullopt, .source = _pickSource},
	                                       object, gripped);
}

bool HandSystem::CreatureTakesPress(entt::entity object) const noexcept
{
	const auto* creature = std::as_const(Locator::entitiesRegistry::value()).TryGet<const Creature>(object);
	if (creature == nullptr || !Locator::creatureHandSystem::has_value())
	{
		return false;
	}
	return creature_hand::TakesPress({
	    .isCreature = true,
	    .mayHold = Locator::creatureHandSystem::value().MayHold(object),
	    .cannotBePickedUp = ecs::object_flags::IsCannotBePickedUp(object),
	    .locked = _creatureLock.has_value(),
	    .friendly =
	        creature_hand::IsFriendlyTo(creature->owner, PlayerNames::PLAYER_ONE, ecs::alliances::AlliancesOf(creature->owner)),
	});
}

void HandSystem::UpdateCreatureLock(bool actionReleased) noexcept
{
	if (!_creatureLock)
	{
		return;
	}
	const auto creature = *_creatureLock;
	const hand_creature::LockFacts facts {
	    .available = ecs::IsAvailable(creature),
	    .locked = _creatureLocked,
	    .stillSelected = _lockedSelectAction == creature,
	    .actionReleased = actionReleased,
	};
	const auto step = hand_creature::StepLock(_creatureLockState, facts);
	if (step.sendEnd)
	{
		SendEndLockedSelect();
	}
	if (!step.letGo)
	{
		_creatureLockState = step.next;
		return;
	}
	// (openblack guard) a creature the hand took hold of without reaching the CREATURE state is let go without
	// feedback: the original sends it only from that state
	if (_renderHandState != hand_creature::k_CreatureHandState && Locator::creatureHandSystem::has_value() &&
	    Locator::creatureHandSystem::value().GetCreature().has_value())
	{
		(void)Locator::creatureHandSystem::value().Release();
	}
	_creatureLock.reset();
	_creatureLocked = false;
	_creatureFinishing = false;
	_creatureLockState = {};
}

void HandSystem::BoxTakesInput() noexcept
{
	// the box's frame: the action state's end, then EndAction, the action state reset and the buttons cleared. The ends:
	// a locked select (a scooping, a fish farm, the creature) sends its end and stops the multi pickup; a grab only
	// stops its immersion, so there is no tap and no pick-up, and a tugged tree stays planted as it stood; a release
	// state takes nothing from the hand. The press the box cut short is over: the hand sees no release of it
	if (_pickPressHeld || _creatureLock)
	{
		SendEndLockedSelect();
	}
	_pickPressHeld = false;
	if (_creatureLock)
	{
		// as the creature's lock is let go (UpdateCreatureLock), without its end packet, sent above
		if (_renderHandState != hand_creature::k_CreatureHandState && Locator::creatureHandSystem::has_value() &&
		    Locator::creatureHandSystem::value().GetCreature().has_value())
		{
			(void)Locator::creatureHandSystem::value().Release();
		}
		if (Locator::creatureHandSystem::has_value())
		{
			Locator::creatureHandSystem::value().GiveCameraBack();
		}
		_creatureLock.reset();
		_creatureLocked = false;
		_creatureFinishing = false;
		_creatureLockState = {};
	}
	_pendingPick.reset();
	_pendingTimer = false;
	_pendingTugHold = false;
	_pendingTugStarted = false;
	_pendingTugFirst = false;
	if (_tug)
	{
		auto& registry = Locator::entitiesRegistry::value();
		if (registry.Valid(*_tug) && registry.AllOf<HandDrawPose>(*_tug))
		{
			registry.Remove<HandDrawPose>(*_tug);
			registry.SetDirty();
		}
		_tug.reset();
		_tugPull.reset();
		_tugHandPoint.reset();
	}
	EndAction();
	_screenGrip = false;
	_actionWasHeld = false;
}

int32_t HandSystem::UpdateCreatureFrame(uint32_t cameraMs) noexcept
{
	const bool creatureInteraction = _interfaceHandState == 0x13;
	if (creatureInteraction)
	{
		// (approximate) the original takes the creature under the hand again every frame; the locked one stands for it
		_creatureInHand = _creatureLock;
	}
	// The interaction camera: on the creature while the interaction lasts, given back on any other state
	if (Locator::creatureHandSystem::has_value())
	{
		auto& creatureHand = Locator::creatureHandSystem::value();
		if (creatureInteraction && _creatureInHand.has_value())
		{
			creatureHand.FollowWithCamera(*_creatureInHand);
		}
		else
		{
			creatureHand.GiveCameraBack();
		}
	}
	const hand_creature::FrameFacts facts {
	    .creatureInteraction = creatureInteraction,
	    .hasCreature = _creatureInHand.has_value(),
	    .cameraMs = cameraMs,
	    .requiredState = RequiredHandState(),
	    .renderState = _renderHandState,
	    .creatureLocked = _creatureLocked,
	    .creatureFinishing = _creatureFinishing,
	};
	const auto ownCreature = [this]() { return CreatureOwnerForHand(*_creatureInHand) == PlayerNames::PLAYER_ONE; };
	const auto step = hand_creature::StepFrame(_creatureInteraction, facts, ownCreature);
	_creatureInteraction = step.interaction;
	for (const auto send : step.Sends())
	{
		switch (send)
		{
		case hand_creature::Send::Click:
			// let go of the player's own creature within a click's time: the click packet, the player's leash key
			game_packets::Push({game_packets::Type::CreatureLeashClick});
			break;
		case hand_creature::Send::Feedback:
			LeaveCreatureState();
			break;
		}
	}
	return step.renderState;
}

void HandSystem::LeaveCreatureState() noexcept
{
	// how the creature was treated goes to its mind at the next turn. (approximate) the original sends nothing when
	// the creature is ready to be let go already
	if (_creatureInHand && Locator::creatureHandSystem::has_value())
	{
		if (const auto given = Locator::creatureHandSystem::value().Release())
		{
			game_packets::Packet packet {game_packets::Type::CreatureFeedback, given->creature};
			packet.data[0] = given->feedback;
			game_packets::Push(packet);
		}
	}
	_creatureInHand.reset();
}

void HandSystem::ProcessLockedSelect() noexcept
{
	// The locked select: its turn count goes up; it lasts while the object is available, has influence > 0 at the
	// synced hand and its interaction (pile, field, fish farm) returns 1; otherwise it is cleared and the action ends
	// (the select's end, then EndAction, for every kind of object). First, a locked object that is no longer available
	// is let go of, whatever it is, and one that is not a creature also once the hand has no influence; for a creature or
	// a totem the action ends with it (the select's end, which drops a totem and sends nothing for an object that is
	// gone, then EndAction), where the pass of the source the hand takes from ends its own below
	if (_statusLockedObject)
	{
		const auto object = *_statusLockedObject;
		const bool available = ecs::IsAvailable(object);
		const bool creature = available && std::as_const(Locator::entitiesRegistry::value()).AllOf<Creature>(object);
		const auto step = hand_creature::StepLockedObject({
		    .available = available,
		    .creature = creature,
		    .influence =
		        available && !creature ? influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, _turnHand) : 0.0f,
		    .pickSource = _statusLockedObject == _pickSource,
		});
		if (step.letGo)
		{
			_statusLockedObject.reset();
		}
		if (step.endAction)
		{
			SendEndLockedSelect();
			EndAction();
		}
	}
	if (!_pickSource)
	{
		return;
	}
	++_pickTurns;
	bool keep = ecs::IsAvailable(*_pickSource) && _held && ecs::IsAvailable(*_held) &&
	            influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, _turnHand) > 0.0f;
	if (keep)
	{
		keep = _pickFish ? ProcessInInteractFish() : _pickField ? ProcessInInteractField() : ProcessInInteractPile();
	}
	if (!keep)
	{
		_statusLockedObject.reset();
		_pickSource.reset();
		_pickFish = false;
		_pickField = false;
		_pickTurns = 0;
		// the action ends as for a creature or a totem: the select's end, then the objects under the hand let go of
		SendEndLockedSelect();
		EndAction();
	}
}

void HandSystem::ApplyTap(entt::entity object) noexcept
{
	// The tap packet: the object interactable and valid to tap, then tapped at the status's synced hand (abode, rock,
	// scaffold, one-off spell seed). The help profile's Tap trigger; a rock is the player's deed of breaking rocks,
	// told to the creature before the rock splits
	if (!Interactable(object))
	{
		return;
	}
	const pot_resource::Dropper is {true, PlayerNames::PLAYER_ONE, true};
	if (!hand_tap::ValidToTap(object, is))
	{
		return;
	}
	help_profile::Trigger(help_profile::Event::Tap);
	// the deed is told for a thing that answers it is a rock
	if (object_physics::IsRock(object))
	{
		creature_mimic::Consider(PlayerNames::PLAYER_ONE, creature_watching::Deed::BreakRocks, object);
	}
	hand_tap::Tap(object, is, _turnHand);
}

void HandSystem::ResetTurnState(bool newGame) noexcept
{
	// the hand's part of the map's clear, at every land load: the last released object and its countdown, and the
	// object the drawn hand holds, without letting it go
	_lastReleased.reset();
	_releaseImpulseMs = 0;
	_renderHandHeld.reset();
	_renderHandHadNeedsSorting = false;
	// then the interface's check of its action, with every thing of the last land gone
	ValidateAtClearMap();
	// the status pass lets go of the locked object, gone with the last land
	_statusLockedObject.reset();
	if (!newGame)
	{
		// a land change keeps the interface status: its synced hand, turn motion, last-sent values and throw
		return;
	}
	// A new game makes the interface status again with its hands empty: a held object and the locked select's are
	// forgotten with no out-of-hand call
	_held.reset();
	_pickSource.reset();
	_pickFish = false;
	_pickField = false;
	_pickTurns = 0;
	_releaseArmed = false;
	// A new game makes the interface status again: the synced and turn fields; with them the hand sync's last-sent
	// values and countdown, and the status's throw
	_syncMapCoords = {};
	_syncHand = glm::vec3(0.0f);
	_syncCameraPosition = glm::vec3(0.0f);
	_syncCameraFocus = glm::vec3(0.0f);
	_turnHand = glm::vec3(0.0f);
	_turnDelta = glm::vec3(0.0f);
	_stillMotion = glm::vec3(0.0f);
	_stillTurns = 0;
	_turnSpeed = 0.0f;
	_turnHeading = 0.0f;
	_turnRate = 0.0f;
	_turnVelocity = glm::vec3(0.0f);
	_turnSideAcceleration = 0.0f;
	_sentMapCoords = {};
	_sentHand = glm::vec3(0.0f);
	_sentCameraFocus = glm::vec3(0.0f);
	_sentCameraPosition = glm::vec3(0.0f);
	_syncCountdown = 0;
	_statusThrowVelocity = glm::vec3(0.0f);
	_statusThrowHandPosition = glm::vec3(0.0f);
	_statusThrowAngular = glm::vec3(0.0f);
	_statusThrowAngles = glm::vec3(0.0f);
}

int32_t HandSystem::ActionStateNumber() const noexcept
{
	// openblack keeps the action's state in its own fields; this is the original's number for them. The locked selects
	// (3 to 6) need the same, as do the idle states
	if (_lockedSelectAction || _creatureLock)
	{
		return 3;
	}
	if (_seedAction != SeedAction::None)
	{
		return static_cast<int32_t>(_seedAction);
	}
	if (_applyObjectState)
	{
		return 18;
	}
	if (_pendingPick)
	{
		return 13;
	}
	if (_pickPressHeld && !_held)
	{
		return 7;
	}
	if (_releaseArmed)
	{
		return 12;
	}
	if (_giving.Reaching())
	{
		return 15;
	}
	if (_screenGrip)
	{
		return 17;
	}
	return interface_active::IsActive() ? 0 : ecs::hand_validate::k_InactiveState;
}

void HandSystem::ValidateAtClearMap() noexcept
{
	// The map's clear checks the action against its objects once the land's things are deleted: none of them can be
	// acted on any more. The action's object is the one its collide holds, the locked select's during one
	const auto* helpSystem = help::Get();
	const auto actionObject = _collideObject.has_value() || _lockedSelectAction.has_value() || _creatureLock.has_value();
	const auto outcome = ecs::hand_validate::Validate({
	    .active = interface_active::IsActive(),
	    .scriptHoldsWideScreen = helpSystem != nullptr && helpSystem->IsScriptWideScreen(),
	    .demoPlaying = hand_demo::IsPlaying(),
	    .state = ActionStateNumber(),
	    .actionObject = actionObject,
	    .actionObjectInteractable = false,
	    .actionObjectInHand = false,
	    .holdsInteractable = false,
	});
	if (outcome == ecs::hand_validate::Outcome::Keep)
	{
		return;
	}
	if (outcome == ecs::hand_validate::Outcome::SetInactive)
	{
		interface_active::SetActive(false);
	}
	// The state's end, with its objects deleted: a locked select sends no end packet (its object is unavailable) and
	// only the multi pickup stops; a locked apply sends no unlock (its seed is unavailable); a grab only stops its force
	// feedback, so nothing is tapped or picked up
	if (_lockedSelectAction)
	{
		_lockedSelectAction.reset();
		_lockedSelectStopped = true;
	}
	_creatureLock.reset();
	_creatureLocked = false;
	_creatureFinishing = false;
	_creatureLockState = {};
	_pickPressHeld = false;
	_pendingPick.reset();
	_pendingTimer = false;
	_pendingTugHold = false;
	_pendingTugStarted = false;
	_pendingTugFirst = false;
	_tug.reset();
	_tugPull.reset();
	_tugHandPoint.reset();
	_seedAction = SeedAction::None;
	// then the action's own end: the action's objects cleared, the apply gate, the button bits
	EndAction();
	_screenGrip = false;
	_actionWasHeld = false;
}

void HandSystem::ProcessStatusAtClearMap(bool newGame) noexcept
{
	// The map's clear runs the interface status's pass once, after the check of the action, with every thing of the
	// last land deleted. First the hand's movement, as every turn
	UpdateTurnMovement();
	// then what the hand keeps of its player's influence past the border, with no citadel, town or ring left on the
	// land (a new game's interface status makes it afresh)
	if (Locator::influenceSystem::has_value())
	{
		Locator::influenceSystem::value().ProcessVirtualInfluenceAtClearMap(_syncMapCoords, newGame);
	}
	// then, for the local interface, the guidance's heart beat, which finds no town and no citadel left
	if (Locator::audioState::has_value())
	{
		audio::guidance::UpdateHeartBeatAtClearMap();
	}
	// The status's locked object is deleted: it is let go and the action ends. The check before has ended the locked
	// select's action already, so no end packet goes
	if (_pickSource)
	{
		_pickSource.reset();
		_pickFish = false;
		_pickField = false;
		_pickTurns = 0;
		EndAction();
	}
	// Then the hands: a held object, deleted, leaves the hand with its out-of-hand calls. The drawn hand let go of it
	// at the hand's clear, so nothing is thrown; with the hand empty nothing more is done
	if (_held)
	{
		TakeDeletedOutOfHand();
	}
}

void HandSystem::ValidateHands() noexcept
{
	// A held object that is no longer interactable (deleted: ToBeDeleted marks it, the deletion comes later in the turn)
	// leaves the hand with no physics (the fire's SetOutMagicHand; a seed's own out-of-hand hook); then the hands are
	// tidied and, for the local interface, the hand's own hold is thrown (here the same _held)
	if (!_held || Interactable(*_held))
	{
		return;
	}
	TakeDeletedOutOfHand();
}

void HandSystem::TakeDeletedOutOfHand() noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = *_held;
	_held.reset();
	_pickSource.reset();
	_releaseArmed = false;
	_seedAction = SeedAction::None;
	RenderHandRelease();
	if (registry.Valid(entity))
	{
		// Out of the hand: the in-hand flag cleared, the object's out-of-hand hook, the fire's SetOutMagicHand. Only a
		// seed's hook does anything for a deleted object: every other object's is the fire's alone, and a scaffold's
		// leaves no surplus once its deletion has taken its planned building
		if (registry.AllOf<SpellSeed>(entity))
		{
			SeedLeftHand(entity);
		}
		ecs::fire::SetOutMagicHand(entity);
	}
}

void HandSystem::ProcessVirtualInfluence() const noexcept
{
	if (Locator::influenceSystem::has_value())
	{
		Locator::influenceSystem::value().ProcessVirtualInfluence(_syncMapCoords);
	}
}

glm::vec3 HandSystem::SyncMapPoint() const noexcept
{
	return map_coords::ToWorld(_syncMapCoords);
}

bool HandSystem::HandCastNeedsContinualPackets() noexcept
{
	// The spells, until one needs continual packets: cast from the hand, not closed down, cast by a human player and
	// the status's player.
	// (pending) the flock spell's override (one more case first)
	bool needs = false;
	Locator::entitiesRegistry::value().Each<const Spell>([&needs](entt::entity spell, const Spell& data) {
		needs = needs || (magic::IsCastFromHand(spell) && !data.closedDown && data.isHumanPlayerCasting &&
		                  data.player == PlayerNames::PLAYER_ONE);
	});
	return needs;
}

void HandSystem::SendHandSync() noexcept
{
	// While the countdown is not 0 and no hand cast needs continual packets, it goes down by one and nothing is
	// compared
	if (_syncCountdown != 0 && !HandCastNeedsContinualPackets())
	{
		--_syncCountdown;
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// the action collide's MapCoords (the map's corner when the point is cleared), the hand, the camera position and
	// ((inferred) the focus)
	_actionCoords = map_coords::FromWorld(_interactionPoint.value_or(glm::vec3(0.0f)));
	const auto hand = registry.Get<const Transform>(_hands[static_cast<size_t>(Side::Left)]).position;
	glm::vec3 cameraPosition = _sentCameraPosition;
	glm::vec3 cameraFocus = _sentCameraFocus;
	if (Locator::camera::has_value())
	{
		cameraPosition = Locator::camera::value().GetOrigin();
		cameraFocus = Locator::camera::value().GetFocus();
	}
	// a component differs by more than 0.01 (the float difference against the double)
	const auto moved = [](glm::vec3 a, glm::vec3 b) {
		constexpr double k_Epsilon = 0.0099999997764825821;
		return static_cast<double>(std::fabs(a.x - b.x)) > k_Epsilon || static_cast<double>(std::fabs(a.y - b.y)) > k_Epsilon ||
		       static_cast<double>(std::fabs(a.z - b.z)) > k_Epsilon;
	};
	// another map cell (CellX / CellZ, the high words)
	const bool handChanged = map_coords::CellX(_sentMapCoords) != map_coords::CellX(_actionCoords) ||
	                         map_coords::CellZ(_sentMapCoords) != map_coords::CellZ(_actionCoords) || moved(_sentHand, hand);
	const bool cameraChanged = moved(_sentCameraFocus, cameraFocus) || moved(_sentCameraPosition, cameraPosition);
	game_packets::Packet packet {game_packets::Type::HandAndCamera};
	packet.coords = _actionCoords;
	packet.data = {hand.x,           hand.y,        hand.z,        cameraPosition.x, cameraPosition.y,
	               cameraPosition.z, cameraFocus.x, cameraFocus.y, cameraFocus.z};
	if (handChanged && cameraChanged)
	{
		packet.type = game_packets::Type::HandAndCamera;
	}
	else if (handChanged)
	{
		packet.type = game_packets::Type::Hand;
	}
	else if (cameraChanged)
	{
		packet.type = game_packets::Type::Camera;
	}
	else
	{
		return; // nothing sent, the countdown as it is
	}
	if (handChanged)
	{
		_sentMapCoords = _actionCoords;
		_sentHand = hand;
	}
	if (cameraChanged)
	{
		_sentCameraFocus = cameraFocus;
		_sentCameraPosition = cameraPosition;
	}
	packet.value = NextPingIndex();
	game_packets::Push(packet);
	// 3 in the internet lobby, else 1 ((inferred) single player)
	_syncCountdown = 1;
}

void HandSystem::UpdateTurnMovement() noexcept
{
	// The hand's movement per turn. k = 1000 / the turn's ms, the turns per second. (approximate) float steps in the
	// original's order; the 24-bit FPU's intermediate roundings are not checked one by one
	const float k = 1000.0f / static_cast<float>(game_clock::MsPerTurn());
	const glm::vec3 previous = _turnDelta;
	const glm::vec3 delta = _syncHand - _turnHand; // 1: the hand's move this turn
	_turnDelta = delta;
	_stillMotion += delta; // 2
	if (glm::length(_stillMotion) < 1.0f)
	{
		++_stillTurns; // 3: the hand stayed within one unit
	}
	else
	{
		_stillTurns = 0;
		_stillMotion = glm::vec3(0.0f);
	}
	_turnVelocity += 0.6f * (k * delta - _turnVelocity); // 4
	// 5: the sideways part of this turn's delta, against the last turn's delta turned 90 degrees in XZ
	float side = 0.0f;
	const glm::vec3 normal(previous.z, 0.0f, -previous.x);
	if (normal.x != 0.0f || normal.z != 0.0f)
	{
		if (const float length = glm::length(normal); length != 0.0f)
		{
			side = glm::dot(delta, normal / length);
		}
	}
	_turnSideAcceleration += 0.6f * (side * k * k - _turnSideAcceleration); // 6
	_turnSpeed = glm::length(_turnVelocity);                                // 7
	_turnHeading = std::atan2(_turnVelocity.z, _turnVelocity.x);            // 8: 0..2 pi
	if (_turnHeading < 0.0f)
	{
		_turnHeading += glm::two_pi<float>();
	}
	_turnRate = _turnSpeed > 0.0001f ? -(_turnSideAcceleration / _turnSpeed) : 0.0f; // 9
	_turnHand = _syncHand;                                                           // 10
}

void HandSystem::ProcessTurn() noexcept
{
	// The interface's turn (after the superpackets), the hand's part: the hand sync packets, then the status pass: the
	// movement, (elsewhere) the heart beat, the locked select and the hands: the held object takes the synced hand as
	// its place, then its in-hand process
	// the action's objects checked first: in the action state the feed of a fireball left, an object under the cursor
	// that is no longer there (the ball the seed took in) ends the action. (not ported) the same check for the other
	// action states
	if (_applyObjectState && _cursorObject && !Interactable(*_cursorObject))
	{
		EndAction();
	}
	SendHandSync();
	UpdateTurnMovement();
	// then what the hand keeps of its player's influence past the border follows the hand's synced place
	ProcessVirtualInfluence();
	ProcessLockedSelect();
	ValidateHands();
	// The creature locked for the hand finishes the body action it was doing when taken hold, each turn until its body
	// plays none: only then does the hand enter its CREATURE state
	if (_creatureFinishing)
	{
		const auto& registry = std::as_const(Locator::entitiesRegistry::value());
		const auto* animation = _creatureLock.has_value() && registry.Valid(*_creatureLock)
		                            ? registry.TryGet<const CreatureAnimation>(*_creatureLock)
		                            : nullptr;
		const bool bodyActing = animation != nullptr && creature_layers::IsPlaying(animation->body);
		const bool wasFinishing = _creatureFinishing;
		_creatureFinishing = hand_creature::StillFinishing(_creatureFinishing, _creatureLocked, bodyActing);
		// As the creature state begins the body is taken off the game: the game reads its pose of now until it is let go
		if (hand_creature::TakesBodyOffTheGame(wasFinishing, _creatureFinishing, _creatureLocked))
		{
			auto& writable = Locator::entitiesRegistry::value();
			if (auto* pose = writable.Valid(*_creatureLock) ? writable.TryGet<CreatureTurnPose>(*_creatureLock) : nullptr)
			{
				creature_turn_pose::Freeze(*pose);
			}
		}
	}
	// After the status pass: the apply gate opens once its packet's handler has run
	if (_applySentTurn <= _applyHandledTurn)
	{
		_applySentTurn = 0;
		_applyHandledTurn = 0;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!_held || !registry.Valid(*_held))
	{
		return;
	}
	const auto held = *_held;
	// MapCoords x, z = the synced hand in fixed point (map_coords::ToFixed), the altitude over the land there = the
	// hand's height - the altitude (so the place's height is the hand's). Every turn, before the in-hand process: the
	// turn's logic reads it there; the frames between draw it in the hand (UpdateHeldObject, DrawOutOfMap). The land
	// under it is looked up at a second conversion, x times 65536 times 0.1, and the height over it = y - the altitude
	// there (a float); the place's height is then the altitude at the MapCoords plus that
	if (auto* transform = registry.TryGet<Transform>(held); transform != nullptr)
	{
		const float x = map_coords::Quantise(_turnHand.x);
		const float z = map_coords::Quantise(_turnHand.z);
		float y = _turnHand.y;
		if (Locator::terrainSystem::has_value())
		{
			const auto ground = land_morph::CurrentAltitude();
			// (inferred) CurrentAltitude (GetHeightAt) stands for the original's altitude
			const auto lookup = [](float m) { return map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(m)); };
			const float altitude = _turnHand.y - ground(glm::vec2(lookup(_turnHand.x), lookup(_turnHand.z)));
			y = ground(glm::vec2(x, z)) + altitude;
		}
		transform->position = glm::vec3(x, y, z);
		// (inferred) the drawn matrix's rotation too: the original's turn reads the drawn mesh (FireCentre's mesh centre)
		if (const auto* pose = registry.TryGet<const HandDrawPose>(held); pose != nullptr)
		{
			transform->rotation = pose->rotation;
		}
	}
	if (registry.AllOf<SpellSeed>(held))
	{
		magic::seed::ProcessInHand(held);
	}
	else if (ecs::scaffolds::IsScaffold(held))
	{
		// a scaffold's own in-hand process, in place of the fire check: its phantom turns and fades, and while the hand
		// stays still it chooses the building it offers there. It always stays in the hand
		ecs::scaffolds::ProcessInHand(held, {.player = PlayerNames::PLAYER_ONE, .stillTurns = _stillTurns, .hand = _turnHand});
	}
	else if (influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, ecs::fire::traits::FireCentre(held)) > 0.0f)
	{
		// Inside the holder's influence it catches the fires it is held over
		// (inferred: openblack has only the local player's hand, taken as PLAYER_ONE)
		ecs::fire::CheckToSeeIfObjectIsNearOnFireObject(held);
	}
}
