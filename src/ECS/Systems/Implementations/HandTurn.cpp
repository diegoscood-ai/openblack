/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's packets (game_packets: sent by the interface, applied at the next turn's start) and the hand's per-turn
// pass (GInterface::Process 0x5CEC10 -> fn_005D2250 -> fn_005CEBB0 -> GInterfaceStatus::Process 0x5DC4E0). Research:
// dev\documentacion\hand\packets\README.md, SPEC.md and dev\documentacion\hand\turnhand\README.md.

#define LOCATOR_IMPLEMENTATIONS

#include "HandSystem.h"
#include "HandSystemDetail.h"

#include <glm/gtc/constants.hpp>

#include "Camera/Camera.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Influence/Influence.h"
#include "ECS/MapCoords.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandTap.h"
#include "ECS/ThingFlags.h"
#include "ECS/ToBeDeleted.h"
#include "GameClock.h"
#include "Input/GamePackets.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellSeed.h"
#include "PSys/PSysManager.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

HandSystem::~HandSystem()
{
	game_packets::Reset();
	game_packets::ClearHandlers();
}

void HandSystem::RegisterPacketHandlers() noexcept
{
	using game_packets::Packet;
	using game_packets::Type;
	// 0x20 -> 0x5DA650: the object interactable and InterfaceValidToTap(status) == 1 again, then InterfaceTap(status)
	game_packets::SetHandler(Type::Tap, [this](const Packet& packet) { ApplyTap(packet.object); });
	// 0x15 -> 0x5DBFB0, 0x16 -> fn_005DC060, 0x17 -> 0x5DC0D0 (GPacket::ProcessPacket 0x63CA86..0x63CB9C): the synced
	// MapCoords and hand, the synced camera. (not ported) the ping ring index (+0x11C, debug only) and the copies +0xD4
	// <- +0xC8, +0xE4 <- +0xE0, +0xE0 = 0 (no reader found)
	game_packets::SetHandler(Type::HandAndCamera, [this](const Packet& packet) {
		_syncMapCoords = packet.coords;
		_syncHand = glm::vec3(packet.data[0], packet.data[1], packet.data[2]);
		_syncCameraPosition = glm::vec3(packet.data[3], packet.data[4], packet.data[5]);
		_syncCameraFocus = glm::vec3(packet.data[6], packet.data[7], packet.data[8]);
	});
	game_packets::SetHandler(Type::Hand, [this](const Packet& packet) {
		_syncMapCoords = packet.coords;
		_syncHand = glm::vec3(packet.data[0], packet.data[1], packet.data[2]);
	});
	game_packets::SetHandler(Type::Camera, [this](const Packet& packet) {
		_syncCameraPosition = glm::vec3(packet.data[3], packet.data[4], packet.data[5]);
		_syncCameraFocus = glm::vec3(packet.data[6], packet.data[7], packet.data[8]);
	});
	// 0x4D: memcpy of the 0x30 bytes to GInterfaceStatus +0x44 (velocity +0x44, angular velocity +0x50, position +0x5C,
	// YXZ angles +0x68): the throw that the next 0x12 / 0x1D / 0x11 uses. (not ported) the angular velocity, which
	// from_hand does not take yet
	game_packets::SetHandler(Type::ThrowData, [this](const Packet& packet) {
		_statusThrowVelocity = glm::vec3(packet.data[0], packet.data[1], packet.data[2]);
		_statusThrowHandPosition = glm::vec3(packet.data[6], packet.data[7], packet.data[8]);
		_statusThrowRotation = packet.rotation;
	});
	// 0x12 -> 0x5DA400: the held object's ApplyThisToMapCoord at the packet's MapCoords (0x5DA405..0x5DA42A), then
	// ThrowObjectFromHand with the status's throw
	game_packets::SetHandler(Type::ApplyToMapCoord, [this](const Packet& packet) {
		if (!_held || !ecs::IsAvailable(*_held))
		{
			return; // 0x5DA474: the held object IsAvailable (vt 0x2C), else nothing
		}
		if (IsHoldingSeed())
		{
			ApplySeedToMapCoord(packet);
		}
		else
		{
			Release(_statusThrowVelocity, packet.position, true);
		}
	});
	// 0x13 -> GInterface::PlaceObjectInMagicHand 0x5DA6F0: the checks again, then InterfaceSetInMagicHand (vt 0x700);
	// a refusal or a NULL object: EndAction fn_005D1260 (0x5DA88F)
	game_packets::SetHandler(Type::PlaceInHand, [this](const Packet& packet) {
		// an object, IsInteractable (0x5DA705), IsSpaceInHands (0x5DA719), ValidForPlaceInHand (0x5DA73B),
		// IsCannotBePickedUp (0x5DA74E). (pending) the flags +0x24 & 4 (0x5DA726), +0x24 & 0x20 (0x5DA75C) and
		// +0xA & 0x10 (0x5DA766), not identified
		if (packet.object == entt::null || !Interactable(packet.object) || _held || !ValidForPlaceInHand(packet.object) ||
		    ecs::thing_flags::IsCannotBePickedUp(packet.object))
		{
			EndAction();
			_pickPressHeld = false;
			return;
		}
		// (pending) FireEffect::StartedMoving (0x5DA79F), the reaction 0x10 (0x5DA7DD) and HelpProfile::Trigger 2 / 3
		// (0x5DA817 / 0x5DA82C), with Intro's help_profile
		PickUp(packet.object, false);
	});
	// 0x11 -> 0x5DA1A0: the held object's ApplyThisToObject (a seed's: HandSpellSeed.cpp); a NULL target: EndAction
	// (0x5DA2B1)
	game_packets::SetHandler(Type::ApplyToObject, [this](const Packet& packet) {
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
	// 0x1B -> 0x5DA950 and 0x1C -> 0x5DAA10: the locked select's start and end
	game_packets::SetHandler(Type::StartLockedSelect, [this](const Packet& packet) { ApplyStartLockedSelect(packet.object); });
	game_packets::SetHandler(Type::EndLockedSelect, [this](const Packet& packet) { ApplyEndLockedSelect(packet.object); });
	// 0x1D -> 0x5DA8F0: the held object's ThrowObjectFromHand(status, 1) with the 0x4D's zero velocity
	game_packets::SetHandler(Type::ThrowHeld, [this](const Packet&) { ApplyForceDropHeld(); });
	// 0x2B -> 0x63D6D8: GParticleContainer::CreateSpotVisual(pos, type, 1.0f, NULL) 0x63E540
	game_packets::SetHandler(Type::SpotVisual, [](const Packet& packet) {
		psys::manager::CreateSpotVisual(packet.value, packet.position, 0.0f, entt::null);
	});
}

bool HandSystem::Interactable(entt::entity object) noexcept
{
	// IsAvailable (GameThing 0x401810: !(+0xA & 1), ecs::IsAvailable); Abode::IsInteractable 0x407200 also needs
	// GetPercentBuilt != 0
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !ecs::IsAvailable(object))
	{
		return false;
	}
	return !registry.AllOf<Abode>(object) || abodes::GetPercentBuilt(object) != 0.0f;
}

void HandSystem::PushThrowData(glm::vec3 velocity) noexcept
{
	// The 0x4D's 0x30 bytes (0x5D50C2..0x5D512B): the velocity (+0x44), the angular momentum (+0x50; (not ported) 0,
	// as from_hand), the position (+0x5C) and the GetYXZ angles (+0x68) of the held object at the send.
	// (pending, Fisicas's from_hand::PredictRelease) state 12 takes them from PredictionPhysOb 0xD47088 (Initialise +
	// SetUpPhysOb) after AdjustToGroundLevel 0x5D505C (redone by InitialisePhysicsFromHand's own, so the same result)
	// and fn_00644F20(min([0xD44454] / 100, 5)): the ping of the last own 0x15 / 0x16 / 0x17 is about one turn in
	// single player, so (inferred) the original takes 0 or 1 prediction turn depending on the timer; 0 kept here, the
	// held Transform as it is. Research: dev\documentacion\hand\throwpose\README.md
	game_packets::Packet data {game_packets::Type::ThrowData};
	data.data[0] = velocity.x;
	data.data[1] = velocity.y;
	data.data[2] = velocity.z;
	auto& registry = Locator::entitiesRegistry::value();
	if (_held && registry.Valid(*_held))
	{
		const auto& transform = registry.Get<const Transform>(*_held);
		data.data[6] = transform.position.x;
		data.data[7] = transform.position.y;
		data.data[8] = transform.position.z;
		// LHMatrix::GetYXZ 0x7FAB30 (0x5D512B) of the rotation: see Packet::rotation
		data.rotation = transform.rotation;
	}
	game_packets::Push(data);
}

void HandSystem::SendRelease(glm::vec3 velocity) noexcept
{
	// State 12 0x5D4DB0: packet 0x4D with the throw (0x5D513D), then DropOnMapCoord 0x5D1850 -> SendApplyToMapCoord
	// 0x5D3340 -> packet 0x12 (0x5D362D). (pending) the refusal: off the map or not ValidToApplyThisToMapCoord, the
	// original sends the 0x12 without the 0x4D (0x5D4DF1 / 0x5D4E16 -> 0x5D5150) and 0x5DA400 refuses it (0x5DA48B):
	// the object stays in the hand
	PushThrowData(velocity);
	game_packets::Push({game_packets::Type::ApplyToMapCoord, entt::null, _interactionPoint.value_or(glm::vec3(0.0f))});
}

void HandSystem::SendPlaceInHand(entt::entity entity) noexcept
{
	// GenericPickup 0x5D2800: its sounds at once (0x5D2881..0x5D295D), the packet 0x13 (0x5D2864) and action state 7
	// (WAIT FOR PLACE IN HAND: the press is kept until the object is in the hand, State_WaitPickup 0x5D4A90)
	// +0x24 & 0x40 IN_PHYSICS: flying, not a resting proxy (PhysicsObjects::IsFlying)
	GenericPickupSounds(entity, physics::PhysicsObjects::IsFlying(entity));
	game_packets::Push({game_packets::Type::PlaceInHand, entity});
	_pickPressHeld = true;
}

void HandSystem::SendStartLockedSelect(entt::entity object) noexcept
{
	// GInterface::StartLockedSelect 0x5D1950: the object's NetworkUnfriendlyStartLockedSelect (vt 0x6D4; Object 0x4027D0
	// does nothing, (not ported) the totems'), packet 0x1B (0x5D1985) and action state 3 (fn_005D2980(3)) with the object
	// as the action's (+0x400)
	game_packets::Push({game_packets::Type::StartLockedSelect, object});
	_lockedSelectAction = object;
}

void HandSystem::SendEndLockedSelect() noexcept
{
	// State 3's end 0x5D4870: with an action object (+0x400), its GetReadyForNetworkUnfriendlyEndLockedSelect and
	// NetworkUnfriendlyEndLockedSelect (vt 0x6E0 / 0x6E8, nothing for piles, fields and fish farms) and packet 0x1C
	// (0x5D48A8); then PSysGlobal::StopMultiPickup at once (0x5D48B4). (not verified) The test of the object's byte
	// +0xA bit 1 (0x5D487E)
	if (_lockedSelectAction)
	{
		game_packets::Push({game_packets::Type::EndLockedSelect, *_lockedSelectAction});
		_lockedSelectAction.reset();
		_lockedSelectStopped = true;
	}
}

void HandSystem::ApplyStartLockedSelect(entt::entity object) noexcept
{
	// 0x5DA950 (packet 0x1B): an interactable object not in a locked select already (Flags +0x24 & 0x10; (inferred)
	// openblack has the one hand, so its +0x3C), then NetworkFriendlyStartLockedSelect (vt 0x6D0), the status's locked
	// object (0x5DC110) and Flags |= 0x10. Otherwise EndAction fn_005D1260 (state 3's end: the 0x1C).
	// (inferred) a hand already holding something refuses it: state 3 begins only with an empty hand (ActionPressed)
	auto& registry = Locator::entitiesRegistry::value();
	if (_pickSource == object || _held || !Interactable(object)) // 0x5DA965 IsInteractable
	{
		SendEndLockedSelect();
		return;
	}
	// NetworkFriendlyStartLockedSelect: Field 0x529900, FishFarm 0x52D770, PileResource 0x66E710 (the first amount into
	// a hand pot put in the hand, PlaceObjectInMagicHand 0x5DC870, and PSysGlobal::StartMultiPickup 0x68F8C0)
	// the hand pot at the status's MapCoords +0x14 (Pile 0x66E79F, Field 0x52998B, FishFarm 0x52D80F)
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
		PickUp(object, false, point);
	}
	if (!_pickSource)
	{
		// (inferred) nothing was taken: the same turn's GInterfaceStatus::Process finds no hand pot to fill, its
		// ProcessInInteract returns 0 and the select ends (EndAction)
		SendEndLockedSelect();
		return;
	}
	_lockedSelectStopped = false;
}

void HandSystem::ApplyEndLockedSelect(entt::entity object) noexcept
{
	// a NULL object (gone): EndAction (0x5DAA2A) and nothing else; the turn's pass then ends the select (IsAvailable)
	if (object == entt::null)
	{
		SendEndLockedSelect();
		return;
	}
	// 0x5DAA10 (packet 0x1C): the object's NetworkFriendlyEndLockedSelect (vt 0x6EC; PileResource 0x66E850:
	// StopMultiPickup and the amount tooltip, HandSystem::Update's 0xEEA), the status's locked object cleared (0x5DC110(0))
	// and Flags &= ~0x10
	_pickSource.reset();
	_pickFish = false;
	_pickField = false;
}

void HandSystem::ProcessLockedSelect() noexcept
{
	// GInterfaceStatus::Process 0x5DC4E0, the locked select (+0x3C): +0x40 goes up; it lasts while the object
	// IsAvailable, has influence > 0 at MapCoords(+0xC8) (0x5DC540 -> 0x5DC558) and its ProcessInInteract
	// (vt 0x808: Pile 0x66E520, Field 0x529730, FishFarm 0x52D950) returns 1; otherwise +0x3C = +0x40 = 0 and
	// EndAction fn_005D1260 (state 3's end: the 0x1C)
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
		_pickSource.reset();
		_pickFish = false;
		_pickField = false;
		_pickTurns = 0;
		SendEndLockedSelect();
	}
}

void HandSystem::ApplyTap(entt::entity object) noexcept
{
	// 0x5DA650 (packet 0x20): the object interactable (0x5DA664) and InterfaceValidToTap == 1, then InterfaceTap at the
	// status's synced hand +0xC8 (Abode 0x4068C6, Rock 0x6E749C, Scaffold 0x6E9DF7, OneOffSpellSeed 0x72A689).
	// (pending) HelpProfile::Trigger(8) (0x5DA6A0); (not ported) a rock's ConsiderMakingCreatureMimicPlayer (0x5DA6C8)
	if (!Interactable(object))
	{
		return;
	}
	const pot_resource::Dropper is {true, PlayerNames::PLAYER_ONE, true};
	if (!hand_tap::ValidToTap(object, is))
	{
		return;
	}
	hand_tap::Tap(object, is, _turnHand);
}

void HandSystem::ResetTurnState() noexcept
{
	// GInterfaceStatus::SetToZero 0x5DBA00 (0x5DBA12..0x5DBAB0): the synced and turn fields; with them fn_005D2250's
	// last-sent values and countdown, the status's throw (+0x44..+0x70) and the action's object. (inferred) on a new
	// land, with game_packets::Reset (GInterface::SetToZero 0x5CE4D0's callers not traced)
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
	_statusThrowRotation = glm::mat3(1.0f);
	_lockedSelectAction.reset();
	_lockedSelectStopped = false;
}

void HandSystem::ValidateHands() noexcept
{
	// GInterfaceStatus::ValidateHands 0x5DC610 -> GMagicHand::Validate 0x5FB130: a held object that is no longer
	// IsInteractable (deleted: ToBeDeleted marks it, the deletion comes later in the turn) leaves the hand with no
	// physics, RemoveFromHand 0x5FB0B0 (FireEffect::SetOutMagicHand; a seed's InterfaceSetOutMagicHand); then TidyHands
	// 0x5DC6F0 and, for the local interface, CHand::ThrowObject 0x46DDD0 (the CHand's own hold, here the same _held)
	if (!_held || Interactable(*_held))
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = *_held;
	_held.reset();
	_pickSource.reset();
	_releaseArmed = false;
	_seedAction = SeedAction::None;
	if (registry.Valid(entity))
	{
		// RemoveFromHand 0x5FB0B0: +0x24 &= ~4, InterfaceSetOutMagicHand (vt 0x704), FireEffect::SetOutMagicHand.
		// (pending) vt 0x704 of the other classes (the villager's: Personas' hook), as in RemoveFirstFromHand
		if (registry.AllOf<SpellSeed>(entity))
		{
			SeedLeftHand(entity);
		}
		ecs::fire::SetOutMagicHand(entity);
	}
}

glm::vec3 HandSystem::SyncMapPoint() const noexcept
{
	return ecs::map_coords::ToWorld(_syncMapCoords);
}

bool HandSystem::HandCastNeedsContinualPackets() noexcept
{
	// fn_00721480: the spells (g_game +0x205BC4) until one answers vt 0x514 Spell::NeedsContinualPackets 0x7214C0:
	// IsCastFromHand, not closed down (+0x40), IsHumanPlayerCasting (+0x4C) and the status's player.
	// (pending) SpellFlock's override 0x723280 (one more case first)
	bool needs = false;
	Locator::entitiesRegistry::value().Each<const Spell>([&needs](entt::entity spell, const Spell& data) {
		needs = needs || (magic::IsCastFromHand(spell) && !data.closedDown && data.isHumanPlayerCasting &&
		                  data.player == PlayerNames::PLAYER_ONE);
	});
	return needs;
}

void HandSystem::SendHandSync() noexcept
{
	// fn_005D2250 0x5D237D: while the countdown 0xD18230 is not 0 and no hand cast needs continual packets
	// (fn_005DC810), it goes down by one and nothing is compared
	if (_syncCountdown != 0 && !HandCastNeedsContinualPackets())
	{
		--_syncCountdown;
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// GInterface +0x3F0 (the action collide's MapCoords), CHand +0x78 (the hand), g_camera 0xEA1DB8 (position) and
	// 0xEA1DC4 ((inferred) the focus)
	if (_interactionPoint)
	{
		_actionCoords = ecs::map_coords::FromWorld(*_interactionPoint);
	}
	const auto hand = registry.Get<const Transform>(_hands[static_cast<size_t>(Side::Left)]).position;
	glm::vec3 cameraPosition = _sentCameraPosition;
	glm::vec3 cameraFocus = _sentCameraFocus;
	if (Locator::camera::has_value())
	{
		cameraPosition = Locator::camera::value().GetOrigin();
		cameraFocus = Locator::camera::value().GetFocus();
	}
	// fn_005D2660: a component differs by more than 0.01 ([0x8C7A10], the float difference against the double)
	const auto moved = [](glm::vec3 a, glm::vec3 b) {
		constexpr double k_Epsilon = 0.0099999997764825821;
		return static_cast<double>(std::fabs(a.x - b.x)) > k_Epsilon ||
		       static_cast<double>(std::fabs(a.y - b.y)) > k_Epsilon || static_cast<double>(std::fabs(a.z - b.z)) > k_Epsilon;
	};
	// fn_005D26B0: another map cell (CellX / CellZ, the high words)
	const bool handChanged = ecs::map_coords::CellX(_sentMapCoords) != ecs::map_coords::CellX(_actionCoords) ||
	                         ecs::map_coords::CellZ(_sentMapCoords) != ecs::map_coords::CellZ(_actionCoords) ||
	                         moved(_sentHand, hand);
	const bool cameraChanged = moved(_sentCameraFocus, cameraFocus) || moved(_sentCameraPosition, cameraPosition);
	game_packets::Packet packet {game_packets::Type::HandAndCamera};
	packet.coords = _actionCoords;
	packet.data = {hand.x, hand.y, hand.z, cameraPosition.x, cameraPosition.y, cameraPosition.z,
	               cameraFocus.x, cameraFocus.y, cameraFocus.z};
	if (handChanged && cameraChanged)
	{
		packet.type = game_packets::Type::HandAndCamera; // 0x5D2562
	}
	else if (handChanged)
	{
		packet.type = game_packets::Type::Hand; // 0x5D24E6
	}
	else if (cameraChanged)
	{
		packet.type = game_packets::Type::Camera; // 0x5D2462
	}
	else
	{
		return; // 0x5D245C: nothing sent, the countdown as it is
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
	game_packets::Push(packet);
	// 0x5D261D: fn_005558B0 = 3 when g_game +0x59A8 == 1 (the internet lobby), else 1 ((inferred) single player)
	_syncCountdown = 1;
}

void HandSystem::UpdateTurnMovement() noexcept
{
	// fn_005DBC60 (handsync/README.md §3). k = 1000 / [0xD01A38], the turns per second. (approximate) float steps in the
	// order read there; the 24-bit FPU's intermediate roundings are not checked one by one
	const float k = 1000.0f / static_cast<float>(game_clock::MsPerTurn());
	const glm::vec3 previous = _turnDelta;
	const glm::vec3 delta = _syncHand - _turnHand; // 1: +0xE8 = +0xA4 - +0xC8
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
	_turnVelocity += 0.6f * (k * delta - _turnVelocity); // 4: [0x92ABC8] 0.6
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
	_turnSpeed = glm::length(_turnVelocity);                                  // 7
	_turnHeading = std::atan2(_turnVelocity.z, _turnVelocity.x);              // 8: Atan2Positive 0x7DB770
	if (_turnHeading < 0.0f)
	{
		_turnHeading += glm::two_pi<float>(); // [0x8AB210] 6.2831855f (0x7DB787)
	}
	_turnRate = _turnSpeed > 0.0001f ? -(_turnSideAcceleration / _turnSpeed) : 0.0f; // 9: [0x8BF518]
	_turnHand = _syncHand;                                                          // 10
}

void HandSystem::ProcessTurn() noexcept
{
	// GInterface::Process 0x5CEC10 (ProcessGameInputs 0x54C40D, after ProcessOneSuperpacket), the hand's part:
	// fn_005D2250 (the packets 0x15 / 0x16 / 0x17), then fn_005CEBB0 -> GInterfaceStatus::Process 0x5DC4E0: the
	// movement (fn_005DBC60), (elsewhere) the heart beat, the locked select and ProcessHands 0x5DC6A0 ->
	// GMagicHand::Process 0x5FB190: the held object's ProcessInHand (vt 0x804). (pending) the held object following the
	// synced hand +0xC8 (0x5FB1C5): it follows the hand per frame
	SendHandSync();
	UpdateTurnMovement();
	ProcessLockedSelect();
	ValidateHands();
	auto& registry = Locator::entitiesRegistry::value();
	if (!_held || !registry.Valid(*_held))
	{
		return;
	}
	const auto held = *_held;
	if (registry.AllOf<SpellSeed>(held))
	{
		// SpellSeed::ProcessInHand 0x729930
		magic::seed::ProcessInHand(held);
	}
	else if (influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, ecs::fire::traits::FireCentre(held)) > 0.0f)
	{
		// Object::ProcessInHand 0x639AD0: inside the holder's influence it catches the fires it is held over
		// (inferido: openblack has only the local player's hand, taken as PLAYER_ONE)
		ecs::fire::CheckToSeeIfObjectIsNearOnFireObject(held);
	}
}
