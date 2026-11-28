/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Wonders.h"

#include <cstddef>

#include <array>
#include <optional>
#include <utility>

#include <spdlog/spdlog.h>

#include "Debug/StateHash.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Wonder.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeVillagers.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"

// A wonder's power and what it adds to its player's most tribal power (Wonders.h)

namespace openblack::ecs
{
using namespace components;

namespace
{
constexpr size_t k_Players = static_cast<size_t>(PlayerNames::_COUNT);
constexpr size_t k_Tribes = static_cast<size_t>(Tribe::_COUNT);

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Wonder* WonderComponent(entt::entity wonder)
{
	auto& registry = Entities();
	return wonder != entt::null && registry.Valid(wonder) ? registry.TryGet<Wonder>(wonder) : nullptr;
}

/// The wonder's player: its town's owner, and the neutral player when it has no town
PlayerNames OwnerOf(entt::entity wonder)
{
	auto& registry = Entities();
	const auto town = abode_villagers::TownOf(wonder);
	const auto* t = town != entt::null ? registry.TryGet<const Town>(town) : nullptr;
	return t != nullptr ? t->owner : PlayerNames::NEUTRAL;
}

/// The player's power slot for the wonder's tribe, for AddToPlayer / RemoveFromPlayer, only when built. A building's
/// player is its town's owner, and the neutral player's when it has no town. The tribe is never missing for a wonder
/// with an info record
float* PowerSlot(entt::entity wonder)
{
	const auto owner = OwnerOf(wonder);
	const auto* info = abodes::InfoOf(wonder);
	if (info == nullptr)
	{
		return nullptr;
	}
	const auto player = static_cast<size_t>(owner);
	const auto tribe = static_cast<size_t>(static_cast<int32_t>(info->tribeType));
	if (player >= k_Players || tribe >= k_Tribes || !abodes::IsBuilt(wonder))
	{
		return nullptr;
	}
	// The player's most tribal power for that tribe, which every player's turn copies into its tribal power
	return &magic::players::MagicOf(owner).maxTribalPower.at(tribe);
}
} // namespace

bool wonders::IsWonder(entt::entity entity)
{
	return WonderComponent(entity) != nullptr;
}

void wonders::Create(entt::entity wonder, float scale)
{
	auto& registry = Entities();
	if (wonder == entt::null || !registry.Valid(wonder))
	{
		return;
	}
	// The wonder part on top of the abode, zeroed
	registry.AssignOrReplace<Wonder>(wonder);
	// SetPower with the scale the building is made at
	SetPower(wonder, scale);
	// The creation hooks: the abode's (AbodeArchetype), then two calls on the 3D object ((pending) what they do)
	// Already built -> AddToPlayer
	if (abodes::IsBuilt(wonder))
	{
		AddToPlayer(wonder);
	}
}

void wonders::SetPower(entt::entity wonder, float power)
{
	if (auto* w = WonderComponent(wonder); w != nullptr)
	{
		w->power = power;
	}
}

float wonders::GetPower(entt::entity wonder)
{
	const auto* w = WonderComponent(wonder);
	return w != nullptr ? w->power : 0.0f;
}

void wonders::AddToPlayer(entt::entity wonder)
{
	const auto* w = WonderComponent(wonder);
	if (w == nullptr)
	{
		return;
	}
	// slot = power + slot, as a float
	if (auto* slot = PowerSlot(wonder); slot != nullptr)
	{
		const float before = *slot;
		*slot = w->power + *slot;
		// (openblack) a log line each time a wonder's power is given, to show it in play
		if (auto logger = spdlog::get("game"); logger != nullptr)
		{
			SPDLOG_LOGGER_INFO(logger, "Wonder {}: its power {:.3f} given to player {} for tribe {} ({:.3f} -> {:.3f})",
			                   entt::to_integral(wonder), w->power, static_cast<int>(OwnerOf(wonder)),
			                   static_cast<int>(abodes::InfoOf(wonder)->tribeType), before, *slot);
		}
	}
}

void wonders::RemoveFromPlayer(entt::entity wonder)
{
	const auto* w = WonderComponent(wonder);
	if (w == nullptr)
	{
		return;
	}
	// slot = slot - power, as a float
	if (auto* slot = PowerSlot(wonder); slot != nullptr)
	{
		const float before = *slot;
		*slot = *slot - w->power;
		// (openblack) a log line each time a wonder's power is taken away, to show it in play
		if (auto logger = spdlog::get("game"); logger != nullptr)
		{
			SPDLOG_LOGGER_INFO(logger, "Wonder {}: its power {:.3f} taken from player {} for tribe {} ({:.3f} -> {:.3f})",
			                   entt::to_integral(wonder), w->power, static_cast<int>(OwnerOf(wonder)),
			                   static_cast<int>(abodes::InfoOf(wonder)->tribeType), before, *slot);
		}
	}
}

void wonders::Built(entt::entity wonder)
{
	// After the abode's Built (abodes::Built, which calls this at its end): AddToPlayer; the abode's result is
	// returned
	AddToPlayer(wonder);
}

void wonders::DeleteDependants(entt::entity wonder)
{
	// RemoveFromPlayer, then the abode's DeleteDependants (the caller's)
	RemoveFromPlayer(wonder);
}

std::vector<wonders::TownWonder> wonders::WondersOf(entt::entity town)
{
	std::vector<TownWonder> found;
	if (town == entt::null)
	{
		return found;
	}
	// read through the const registry and the players' magic as it is, so that the readout makes no storage
	std::as_const(Entities()).Each<const Wonder>([&found, town](entt::entity wonder, const Wonder& w) {
		if (abode_villagers::TownOf(wonder) != town)
		{
			return;
		}
		const auto* info = abodes::InfoOf(wonder);
		const auto player = OwnerOf(wonder);
		const auto tribe = info != nullptr ? static_cast<int32_t>(info->tribeType) : -1;
		const auto& powers = magic::players::ReadMagicOf(player).maxTribalPower;
		found.push_back({.wonder = wonder,
		                 .power = w.power,
		                 .built = abodes::IsBuilt(wonder),
		                 .player = player,
		                 .tribe = tribe,
		                 .playersPower = tribe >= 0 && static_cast<size_t>(tribe) < powers.size()
		                                     ? powers.at(static_cast<size_t>(tribe))
		                                     : 0.0f});
	});
	return found;
}

void wonders::RegisterStateHash()
{
	state_hash::Register("wonder_power", [](state_hash::Hasher& h) {
		for (size_t p = 0; p < k_Players; ++p)
		{
			// what the wonders raised the most tribal power by, above its starting 1 (0 while no wonder counts)
			for (const float power : magic::players::MagicOf(static_cast<PlayerNames>(p)).maxTribalPower)
			{
				h.Float(power - 1.0f);
			}
		}
	});
}
} // namespace openblack::ecs
