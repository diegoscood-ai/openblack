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
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/Villager/VillagerDeath.h"
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

uint32_t magic::players::WorldPopulation()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return 0;
	}
	// the Villager entities not counted out (+0xE0 & 0x40: Villager::SetDying 0x76A54C took them off; corpses stay)
	uint32_t count = 0;
	Locator::entitiesRegistry::value().Each<const Villager>([&count](entt::entity villager, const Villager&) {
		if (!ecs::villager::IsCountedOut(villager))
		{
			++count;
		}
	});
	return count;
}

float magic::players::ProportionOfWorldPopulationWhoBelieveInMe(PlayerNames player)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return 0.0f;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	// 0x64B680..0x64B6A8: the town list (+0xA50, next +0x75C): edx += +0x668 (women), esi += +0x664 (men), as integers
	uint32_t women = 0;
	uint32_t men = 0;
	for (const auto entity : ecs::map_cells::TownsOf(player))
	{
		if (const auto* town = registry.TryGet<const Town>(entity); town != nullptr)
		{
			women += town->stats.females;
			men += town->stats.males;
		}
	}
	// 0x64B6AA..0x64B6E1: world (g_game+0x205A54) != 0 and men + women != 0 -> fild qword / fild qword (the FPU at 24
	// bits: a float division of the exact counts)
	const uint32_t world = WorldPopulation();
	const uint32_t believers = men + women;
	if (world == 0 || believers == 0)
	{
		return 0.0f; // 0x64B6E7
	}
	return static_cast<float>(believers) / static_cast<float>(world);
}

void magic::players::Reset()
{
	g_Magic.fill(PlayerMagic {});
	// the alignment is not reset: it lives with the player, not with the land (AlignmentOf)
}
