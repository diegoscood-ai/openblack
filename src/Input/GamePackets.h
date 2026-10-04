/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <cstdint>
#include <functional>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "ECS/MapCoords.h"

// The game's packets as single player runs them: the interface sends (GGame::SendPacketCompressed 0x551690 into the
// buffer g_game +0x5318, flushed by itself at 6 packets, 0x551E0A), the loop flushes once an iteration after the turns
// (fn_005525E0 at 0x54D291 -> LHSession::Write: the session's list) and the next turn applies them first thing
// (ProcessGameInputs 0x54C3D0 -> ProcessOneSuperpacket 0x63C3D0 -> GPacket::ProcessPacket 0x63C420). Nothing is applied
// in the turn that sent it. Research: dev\documentacion\hand\packets\README.md and SPEC.md.
namespace openblack::game_packets
{

/// GPacket's type byte (+1); the handlers of the jump table 0x63DDCC (index type - 6)
enum class Type : uint8_t
{
	ApplyToObject = 0x11,     ///< 0x5DA1A0 ApplyHeldToObject
	ApplyToMapCoord = 0x12,   ///< 0x5DA400 ApplyThisToMapCoord, then the throw
	PlaceInHand = 0x13,       ///< PlaceObjectInMagicHand 0x5DA6F0
	HandAndCamera = 0x15,     ///< 0x5DBF40
	Hand = 0x16,              ///< 0x5DBF70
	Camera = 0x17,            ///< 0x5DBF90
	ApplyUnlock = 0x1A,       ///< 0x5DA8A0
	StartLockedSelect = 0x1B, ///< 0x5DA950 NetworkFriendlyStartLockedSelect
	EndLockedSelect = 0x1C,   ///< 0x5DAA10 NetworkFriendlyEndLockedSelect
	ThrowHeld = 0x1D,         ///< 0x5DA8F0 ThrowObjectFromHand(status, 1)
	Give = 0x1F,              ///< 0x5DA2D0
	Tap = 0x20,               ///< 0x5DA650 InterfaceTap
	SpotVisual = 0x2B,        ///< 0x63D6D8 GParticleContainer::CreateSpotVisual
	ThrowData = 0x4D,         ///< memcpy to GInterfaceStatus +0x44
	SpellIconCharge = 0x6A,   ///< 0x5DAC30
};

/// A decompressed GPacket (0x110 bytes, fn_00551EE0): the fields the interface's packets use
struct Packet
{
	Type type {Type::Tap};
	entt::entity object {entt::null}; ///< +8 (GetObject 0x63DF70: NULL to the handler when the object is gone)
	glm::vec3 position {0.0f};        ///< a MapCoords (+8) or the gesture's point
	int32_t value {0};                ///< +0x14
	/// 0x15 / 0x16: the action collide's MapCoords (GInterface +0x3F0, packet +0x10)
	ecs::map_coords::MapCoords coords {};
	/// 0x4D: velocity, angular momentum, position, YXZ angles (status +0x44..+0x70); 0x11 / 0x12: the gesture
	/// (GestureSystemPacketData: size, point); 0x15 / 0x16 / 0x17: the hand, the camera position and focus
	std::array<float, 16> data {};
	/// 0x4D +0x68: (approximate) the rotation itself where the original sends its LHMatrix::GetYXZ 0x7FAB30 angles,
	/// rebuilt by SetYXZMatrixOnly in Object::ThrowObjectFromHand 0x6385E0: the float round trip is not reproduced.
	/// (pending) lh_matrix::GetYXZ, on Fisicas's lh_matrix::ArcTanOctant (Hito 3 step 6)
	glm::mat3 rotation {1.0f};
};

/// GGame::SendPacketCompressed 0x551690: into the buffer; the buffer flushes by itself once it holds 6 (0x551E0A)
void Push(const Packet& packet);
/// fn_005525E0 (GGame::Loop 0x54D291): the buffer goes to the session's list
void Flush();
/// GNetwork ProcessOneSuperpacket 0x63C3D0: the session's list in order through the handlers (GPacket::ProcessPacket
/// 0x63C420); a packet whose object no longer exists reaches its handler with entt::null (GetObject 0x63DF70 returns 0
/// and the handler takes its NULL path, mostly EndAction)
void ProcessOneSuperpacket();

/// The handler of one packet type (each owner registers its own: the hand all its types)
using Handler = std::function<void(const Packet& packet)>;
void SetHandler(Type type, Handler handler);

/// Packets still waiting (the buffer and the list), for the tests and the traces
[[nodiscard]] size_t Pending();
/// A new game or land: nothing waits (GGame reset; (inferred) the session list is emptied with the game)
void Reset();
/// The handlers' owner goes (the hand system's destructor): no handler is left pointing at it
void ClearHandlers();

} // namespace openblack::game_packets
