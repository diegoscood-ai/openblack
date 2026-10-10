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

#include <entt/entity/entity.hpp>

#include "Input/GamePackets.h"

/// The end of a seed's locked apply (a miracle cast while it is held in the hand). The hand does not unlock the seed
/// at once: it sends the unlock as a packet, after the applies it has already sent, so that the last apply is handled
/// (and its spell cast) before the unlock closes the spell. Wiki: docs/bw1-notes/hand-and-interface.md, "The locked
/// apply's unlock".
namespace openblack::ecs::hand_seed
{

/// The unlock packet for the seed, with the turns the apply was held
[[nodiscard]] inline game_packets::Packet UnlockPacket(entt::entity seed, uint32_t lockTurn, uint32_t turn) noexcept
{
	game_packets::Packet packet {game_packets::Type::ApplyUnlock, seed};
	packet.value = static_cast<int32_t>(turn - lockTurn);
	return packet;
}

} // namespace openblack::ecs::hand_seed
