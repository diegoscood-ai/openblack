/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <functional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

// The game's packets as single player runs them: the interface sends them into a buffer that flushes by itself at 6
// packets, the loop flushes once an iteration after the turns (into the session's list) and the next turn applies them
// first thing. Nothing is applied in the turn that sent it. Wiki:
// docs/wiki/engine/loop.md#the-interface-frames-send-packets-turns-apply-them.
namespace openblack::game_packets
{

/// The packet's type byte, which picks its handler
enum class Type : uint8_t
{
	FightSelectPoint = 0x0F, ///< The grab button pressed on the arena's ground (coords): a step in place of the queue
	FightApplyPoint = 0x10,  ///< The action button pressed on the arena's ground (coords): a step after the queue
	ApplyToObject = 0x11,    ///< Applies the held object to an object
	ApplyToMapCoord = 0x12,  ///< Applies the held object to a map point, then the throw
	PlaceInHand = 0x13,      ///< Puts an object in the hand
	HandAndCamera = 0x15,
	Hand = 0x16,
	Camera = 0x17,
	ApplyUnlock = 0x1A,
	StartLockedSelect = 0x1B, ///< Starts a locked select
	EndLockedSelect = 0x1C,   ///< Ends a locked select
	ThrowHeld = 0x1D,         ///< Throws the held object
	/// The held object (value, by its entity's number) given to the creature (object), reached for with the left hand
	/// when data[0] is 1, the creature turning to the camera at the point data[1], data[2] of the ground
	Give = 0x1F,
	Tap = 0x20, ///< Taps an object
	/// The action button pressed on a fighter's body (object): the move (value, numbered as the scripts number them)
	/// goes into the queue of the sender's creature, after the queued moves
	FightMoveAdd = 0x22,
	/// The grab button pressed on a fighter's body: the move in place of the queued moves
	FightMoveReplace = 0x23,
	/// The button of a press on a fighter's body let go: how long it was held, in ms (value), charges the blow
	FightCharge = 0x24,
	/// The altitude the hand holds a puzzle totem at, its object, in coords (sent at each new step of the drag)
	PuzzleTotemPosition = 0x28,
	/// The share a village totem was let go at, its object, in data[0]
	TotemShare = 0x29,
	SpotVisual = 0x2B,     ///< Spawns a spot visual
	ReleaseImpulse = 0x40, ///< The released object's spin
	ThrowData = 0x4D,      ///< Copied into the interface status's throw data
	/// The hand went too far from the creature reaching for what it gave it (object): the creature gives up taking it
	GiveCallOff = 0x52,
	/// How the hand treated a creature, to its mind (data[0])
	CreatureFeedback = 0x59,
	/// A fight miracle's gesture drawn: the miracle (value, its magic type) goes into the fight queue of the creature
	/// (object), after the queued moves
	FightSpell = 0x5A,
	/// Going inside the citadel: the sender's creature's leash comes off
	LeashOff = 0x5B,
	/// The hand's double click with the leash in it: ties the player's creature's leash to the object, or unties it back
	/// to the hand when the object is null
	LeashTie = 0x5C,
	/// The hand let go of the tied leash's rope it pulled: the player's creature's leash takes data[0] as its full length
	/// and half of it as its slack
	LeashPullLength = 0x3E,
	/// A tap with the leash in the hand: the player's creature acts on the object
	LeashActOnObject = 0x5D,
	/// A tap with the leash in the hand: the player's creature acts on the point of the land (position)
	LeashActOnPoint = 0x5E,
	/// A click on the player's own creature: their leash key
	CreatureLeashClick = 0x5F,
	/// The quick save: the game saved into its quick save's slot
	QuickSave = 0x61,
	/// A hard slap that bruised: the sender's own creature loses a little life
	CreatureHardSlap = 0x63,
	/// A leash picked at the temple's posts: the leash (value) for the sending player
	LeashType = 0x65,
	SpellIconCharge = 0x6A,
};

/// A decompressed packet: the fields the interface's packets use
struct Packet
{
	Type type {Type::Tap};
	entt::entity object {entt::null}; ///< Null to the handler when the object is gone
	glm::vec3 position {0.0f};        ///< A MapCoords or the gesture's point
	int32_t value {0};                ///< HandAndCamera / Hand / Camera: the ping ring index; LeashType: the leash
	/// HandAndCamera / Hand: the action collide's MapCoords
	map_coords::MapCoords coords {};
	/// ThrowData: velocity, angular momentum, position, YXZ angles; ApplyToObject / ApplyToMapCoord: the gesture
	/// (size, point); HandAndCamera / Hand / Camera: the hand, the camera position and focus; TotemShare: the share
	std::array<float, 16> data {};
	/// The player who sent it (single player: the local one)
	PlayerNames player {PlayerNames::PLAYER_ONE};
};

/// Into the buffer; the buffer flushes by itself once it holds 6
void Push(const Packet& packet);
/// Once an iteration of the game loop: the buffer goes to the session's list
void Flush();
/// The session's list in order through the handlers; a packet whose object no longer exists reaches its handler with
/// entt::null (the handler takes its null path, mostly EndAction)
void DispatchQueuedPackets();

/// The handler of one packet type (each owner registers its own: the hand all its types)
using Handler = std::function<void(const Packet& packet)>;
void SetHandler(Type type, Handler handler);

/// Packets still waiting (the buffer and the list), for the tests and the traces
[[nodiscard]] size_t Pending();
/// Nothing waits: the buffer and the session's list emptied (the tests, and the hand system's end)
void Reset();
/// The map's clear (every land load): the session's list is emptied; the buffer keeps what the interface sent since
/// the last flush, which the loop's next flush hands on to the new land (an object gone reaches its handler as null)
void ClearSession();
/// The handlers' owner goes (the hand system's destructor): no handler is left pointing at it
void ClearHandlers();

} // namespace openblack::game_packets
