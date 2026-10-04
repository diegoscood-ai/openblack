/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GamePackets.h"

#include <unordered_map>
#include <utility>
#include <vector>

#include "ECS/Registry.h"
#include "Locator.h"

namespace openblack::game_packets
{
namespace
{
struct State
{
	std::vector<Packet> buffer;  ///< g_game +0x5318 (byte 0: the count)
	std::vector<Packet> session; ///< the session's list (session +0xA4)
	std::unordered_map<uint8_t, Handler> handlers;
};

State& Get()
{
	static State state;
	return state;
}

constexpr size_t k_AutoFlush = 6; ///< 0x551E0A: SendPacketCompressed flushes once the buffer holds 6 packets
} // namespace

void Push(const Packet& packet)
{
	auto& state = Get();
	state.buffer.push_back(packet);
	if (state.buffer.size() >= k_AutoFlush)
	{
		Flush();
	}
}

void Flush()
{
	auto& state = Get();
	state.session.insert(state.session.end(), state.buffer.begin(), state.buffer.end());
	state.buffer.clear();
}

void ProcessOneSuperpacket()
{
	auto& state = Get();
	// the whole list, in the order it was sent; a handler may send new packets: they wait in the buffer for the next flush
	auto packets = std::move(state.session);
	state.session.clear();
	for (auto packet : packets)
	{
		// GPacket::GetObject 0x63DF70: the index and its unique id must still match (an entity's version); otherwise
		// the handler gets NULL (0x63C939 / 0x63C971 / 0x63CC4B pass it on)
		if (packet.object != entt::null &&
		    (!Locator::entitiesRegistry::has_value() || !Locator::entitiesRegistry::value().Valid(packet.object)))
		{
			packet.object = entt::null;
		}
		if (const auto handler = state.handlers.find(static_cast<uint8_t>(packet.type)); handler != state.handlers.end())
		{
			handler->second(packet);
		}
	}
}

void SetHandler(Type type, Handler handler)
{
	Get().handlers[static_cast<uint8_t>(type)] = std::move(handler);
}

size_t Pending()
{
	const auto& state = Get();
	return state.buffer.size() + state.session.size();
}

void Reset()
{
	auto& state = Get();
	state.buffer.clear();
	state.session.clear();
}

void ClearHandlers()
{
	Get().handlers.clear();
}

} // namespace openblack::game_packets
