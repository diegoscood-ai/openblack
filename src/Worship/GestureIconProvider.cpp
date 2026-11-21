/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GestureIconProvider.h"

#include <functional>
#include <memory>

#include "ECS/Registry.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "InfoConstants.h"
#include "Input/GamePackets.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Magic/MagicTables.h"
#include "PlayerSpellIcons.h"
#include "WorshipSpellIcon.h"

using namespace openblack;
using namespace openblack::worship;

namespace
{
/// The interface's player: the human one
PlayerNames InterfacePlayer()
{
	for (int p = 0; p < static_cast<int>(PlayerNames::_COUNT); ++p)
	{
		if (magic::players::IsHuman(static_cast<PlayerNames>(p)))
		{
			return static_cast<PlayerNames>(p);
		}
	}
	return PlayerNames::PLAYER_ONE; // (inferred): the original has no interface status then and does nothing
}

class WorshipIconProvider final: public magic::gestures::IconProvider
{
public:
	[[nodiscard]] bool AnyRequestableIconOfCategory(magic::gestures::Gesture category) const override
	{
		return player::AnyRequestableIconOfCategory(InterfacePlayer(), static_cast<GestureType>(category));
	}

	void ForEachRequestableIcon(magic::gestures::Gesture category,
	                            const std::function<void(int seedType)>& visit) const override
	{
		// The six sites' icons in list order, the category's that are ValidForRequestSpell(status, -1, 1)
		const auto playerName = InterfacePlayer();
		const auto& tables = Locator::infoConstants::value();
		for (const auto icon : player::Icons(playerName))
		{
			const auto seed = icon::SeedTypeOf(icon);
			if (magic::GetSpellSeedInfo(tables, seed).selectionGesture == static_cast<GestureType>(category) &&
			    icon::ValidForRequestSpell(icon, playerName, -1, true))
			{
				visit(static_cast<int>(seed));
			}
		}
	}

	[[nodiscard]] bool IconValidForRequest(int seedType) const override
	{
		return player::IconValidForRequest(InterfacePlayer(), static_cast<SpellSeedType>(seedType));
	}

	void RequestSpell(int seedType) override
	{
		// The selection's request is sent as a packet, applied at the start of the next turn
		game_packets::Push({.type = game_packets::Type::RequestSpell, .value = seedType, .player = InterfacePlayer()});
	}

	[[nodiscard]] bool CanRepeat() const override { return player::CanRepeat(InterfacePlayer()); }

	void RepeatLast(int /*lastSeedType*/) override
	{
		// The repeat is sent as a packet, applied at the start of the next turn with the interface's last seed type then
		game_packets::Push({.type = game_packets::Type::RepeatSpell, .player = InterfacePlayer()});
	}

	[[nodiscard]] bool AnyIconChargingForHand() const override { return player::AnyIconChargingFor(InterfacePlayer()); }

	[[nodiscard]] float MaxChargeFraction() const override { return player::MaxChargeFraction(InterfacePlayer()); }

	void CancelMostChargedIcon() override { player::CancelMostRecentCharge(InterfacePlayer()); }

	[[nodiscard]] bool PowerUpAvailable(entt::entity icon, int powerUp) const override
	{
		return icon != entt::null && Locator::entitiesRegistry::value().Valid(icon) &&
		       icon::PowerUpValid(icon, InterfacePlayer(), powerUp);
	}

	void SetPowerUpCharge(entt::entity icon, int powerUp) override
	{
		player::SetChargingPowerUp(icon, InterfacePlayer(), powerUp);
	}
};
} // namespace

void gesture_icons::Register()
{
	magic::gestures::SetIconProvider(std::make_unique<WorshipIconProvider>());
}

void gesture_icons::RegisterPacketHandlers()
{
	// the request: the seed's plain miracle through the miracles' summon, the one way the worship is asked for a seed:
	// some icon of it can be asked, then the best one is
	game_packets::SetHandler(game_packets::Type::RequestSpell, [](const game_packets::Packet& packet) {
		constexpr int k_PlainMiracle = -1;
		if (Locator::magicSystem::has_value())
		{
			Locator::magicSystem::value().SummonSeed(packet.player, static_cast<SpellSeedType>(packet.value), k_PlainMiracle);
		}
	});
	// the repeat, with the interface's last seed type as the packet is applied. (approximate): openblack keeps a second
	// copy per player (player::SetLastSeedType), overwritten here so that RepeatLastSpell reads the interface's value;
	// the original has no such copy to overwrite
	game_packets::SetHandler(game_packets::Type::RepeatSpell, [](const game_packets::Packet& packet) {
		player::SetLastSeedType(packet.player, static_cast<SpellSeedType>(magic::gestures::State().lastSeedType));
		player::RepeatLastSpell(packet.player);
	});
}
