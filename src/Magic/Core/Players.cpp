/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Players.h"

#include <array>

#include "ECS/Components/Player.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Magic/MagicTables.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
constexpr size_t k_Players = static_cast<size_t>(PlayerNames::_COUNT);
std::array<PlayerMagic, k_Players> g_Magic {};
std::array<PlayerAlignment, k_Players> g_Alignment {};

/// (inferido: guard) the original indexes directly; a bad player falls to the neutral one
size_t IndexOf(PlayerNames player)
{
	const auto index = static_cast<size_t>(player);
	return index < k_Players ? index : static_cast<size_t>(PlayerNames::NEUTRAL);
}

/// (inferido: guard) the original indexes directly; a bad MAGIC_TYPE falls to 0
size_t TypeIndex(MagicType type)
{
	const auto index = static_cast<size_t>(type);
	return index < PlayerMagic::k_MagicTypes ? index : 0;
}
} // namespace

entt::entity magic::players::EntityOf(PlayerNames player)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	entt::entity found = entt::null;
	registry.Each<const Player>([&](entt::entity entity, const Player& component) {
		if (found == entt::null && component.name == player)
		{
			found = entity;
		}
	});
	return found;
}

PlayerMagic& magic::players::MagicOf(PlayerNames player)
{
	const auto entity = EntityOf(player);
	if (entity != entt::null)
	{
		auto& registry = Locator::entitiesRegistry::value();
		if (auto* magic = registry.TryGet<PlayerMagic>(entity); magic != nullptr)
		{
			return *magic;
		}
	}
	return g_Magic[IndexOf(player)];
}

PlayerAlignment& magic::players::AlignmentOf(PlayerNames player)
{
	// GPlayer +0x60 (GAlignment): 0 for a new game (GGame::Init 0x54FEA0 takes the profile's, esi+0xF4 -> +0x1C, 0 without
	// one; GPlayer::LoadPlayerAlignment 0x64D355 reads the registry's, clamped -1..1: no profile in openblack); loading
	// another land does not touch it, so it is kept per PlayerNames, outside the land's registry
	return g_Alignment[IndexOf(player)];
}

bool magic::players::IsHuman(PlayerNames player)
{
	return MagicOf(player).playerType == 1;
}

float magic::players::TribalPower(const GMagicEffectInfo& effect, const PlayerNames* player)
{
	if (player == nullptr)
	{
		return magic::GetTribalPower(effect, nullptr);
	}
	return magic::GetTribalPower(effect, &MagicOf(*player).tribalPower);
}

bool magic::players::IsMagicTypeEnabled(PlayerNames player, MagicType type)
{
	const auto& magic = MagicOf(player);
	return magic.allMagicCheat || magic.remainder[TypeIndex(type)] != 0;
}

void magic::players::SetMagicTypeEnabled(PlayerNames player, MagicType type, bool on)
{
	auto& magic = MagicOf(player);
	auto& remainder = magic.remainder[TypeIndex(type)];
	if (on)
	{
		++remainder;
		magic.everEnabled[TypeIndex(type)] = true;
	}
	else if (remainder > 0)
	{
		--remainder;
	}
}

void magic::players::SetMagicTypeEverBeenEnabled(PlayerNames player, MagicType type)
{
	MagicOf(player).everEnabled[TypeIndex(type)] = true;
}

bool magic::players::HasMagicTypeEverBeenEnabled(PlayerNames player, MagicType type)
{
	return MagicOf(player).everEnabled[TypeIndex(type)];
}

void magic::players::Reset()
{
	g_Magic.fill(PlayerMagic {});
	// the alignment is not reset: it lives with the player, not with the land (AlignmentOf)
}
