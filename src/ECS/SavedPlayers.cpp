/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SavedPlayers.h"

#include <algorithm>
#include <utility>
#include <variant>

#include <WorldReader.h>

#include "Common/HelpText.h"
#include "ECS/AudioQueries.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerAlliances.h"
#include "ECS/Components/PlayerCreatureSlot.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/PlayerAlliances.h"
#include "ECS/Registry.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Locator.h"
#include "Magic/Core/Players.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::saved_players;

namespace
{
/// Copies the values a list holds into an array of openblack's, as far as both go
template <typename To, size_t N, typename From>
void CopyInto(std::array<To, N>& into, const std::vector<From>& from)
{
	for (size_t i = 0; i < std::min(N, from.size()); ++i)
	{
		into.at(i) = static_cast<To>(from.at(i));
	}
}

/// Copies openblack's values into a list, keeping the list's count
template <typename To, typename From, size_t N>
void CopyOut(std::vector<To>& into, const std::array<From, N>& from)
{
	for (size_t i = 0; i < std::min(N, into.size()); ++i)
	{
		into.at(i) = static_cast<To>(from.at(i));
	}
}
} // namespace

PlayerValues saved_players::ValuesOf(const gamesave::PlayerRecord& record)
{
	PlayerValues values;
	values.alignment = {.value = record.alignment.value, .pending = record.alignment.change};
	CopyInto(values.tribalPower, record.tribalPower);
	CopyInto(values.maxTribalPower, record.maxTribalPower);
	values.type = record.type;
	values.allMagic = record.allMagicEnabled != 0;
	values.totalPopulation = record.totalPopulation;
	CopyInto(values.magicEnabled, record.magicTypeEnabled);
	for (size_t i = 0; i < std::min(values.everEnabled.size(), record.magicTypeEverBeenEnabled.size()); ++i)
	{
		values.everEnabled.at(i) = record.magicTypeEverBeenEnabled.at(i) != 0;
	}
	values.influencePower = record.influencePower;
	values.averageInfluencePower = record.averageInfluencePower;
	CopyInto(values.damageFromPlayer, record.damageFromPlayer);
	CopyInto(values.allies, record.allies);
	return values;
}

gamesave::PlayerRecord saved_players::RecordOf(const PlayerValues& values, gamesave::PlayerRecord base)
{
	base.alignment.value = values.alignment.value;
	base.alignment.change = values.alignment.pending;
	CopyOut(base.tribalPower, values.tribalPower);
	CopyOut(base.maxTribalPower, values.maxTribalPower);
	base.type = values.type;
	// the cheat is a whole word the game sets to 1 or 0
	base.allMagicEnabled = values.allMagic ? 1 : 0;
	base.totalPopulation = values.totalPopulation;
	CopyOut(base.magicTypeEnabled, values.magicEnabled);
	CopyOut(base.magicTypeEverBeenEnabled, values.everEnabled);
	base.influencePower = values.influencePower;
	base.averageInfluencePower = values.averageInfluencePower;
	CopyOut(base.damageFromPlayer, values.damageFromPlayer);
	CopyOut(base.allies, values.allies);
	return base;
}

gamesave::PlayerRecord saved_players::NewRecord(PlayerNames player, uint32_t type, std::u16string_view name)
{
	const auto number = static_cast<uint8_t>(player);
	gamesave::PlayerRecord record;
	// the player's own word is its number
	record.thing.extra = number;
	record.playerNumber = number;
	record.alignment = {.classWord = k_AlignmentClassWord, .destroyed = 0, .value = 0.0f, .change = 0.0f};
	record.info = k_PlayerInfo;
	// every tribe's power and most power start at 1
	record.tribalPower.assign(components::PlayerMagic::k_Tribes, 1.0f);
	record.maxTribalPower.assign(components::PlayerMagic::k_Tribes, 1.0f);
	record.type = type;
	record.name.assign(k_NameLength, u'\0');
	std::copy_n(name.begin(), std::min(name.size(), k_NameLength - 1), record.name.begin());
	record.magicTypeEnabled.assign(components::PlayerMagic::k_MagicTypes, 0);
	record.magicTypeEverBeenEnabled.assign(components::PlayerMagic::k_MagicTypes, 0);
	record.damageFromPlayer.assign(k_Players, 0.0f);
	record.allies.assign(k_Players, 0.0f);
	record.interfaces.assign(k_Interfaces, 0);
	return record;
}

std::u16string saved_players::NewName(PlayerNames player, std::u16string_view profileName, std::u16string_view neutralName)
{
	if (player == PlayerNames::PLAYER_ONE)
	{
		return std::u16string(profileName);
	}
	if (player == PlayerNames::NEUTRAL)
	{
		return std::u16string(neutralName);
	}
	std::u16string name = u"Player[";
	name.push_back(static_cast<char16_t>(u'1' + static_cast<uint8_t>(player)));
	name += u"]";
	return name;
}

uint32_t saved_players::NewType(PlayerNames player, PlayerNames localPlayer)
{
	if (player == PlayerNames::NEUTRAL)
	{
		return k_NeutralPlayerType;
	}
	return player == localPlayer ? 1 : 0;
}

gamesave::PlayerRecord saved_players::NewGameRecord(PlayerNames player)
{
	const auto local =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	return NewRecord(player, NewType(player, local),
	                 NewName(player, audio_queries::ProfileName(), helptext::Get(k_NeutralNameText)));
}

PlayerObjects saved_players::ObjectsOf(const Registry& registry, PlayerNames player)
{
	PlayerObjects objects;
	registry.Each<const components::Temple, const components::CitadelWorship>(
	    [&registry, &objects, player](entt::entity entity, const components::Temple& temple,
	                                  const components::CitadelWorship& /*worship*/) {
		    // a citadel marked for deletion is nobody's
		    if (objects.citadel == entt::null && temple.owner == player && !registry.AllOf<components::Unavailable>(entity))
		    {
			    objects.citadel = entity;
		    }
	    });
	registry.Each<const components::Player>(
	    [&registry, &objects, player](entt::entity entity, const components::Player& component) {
		    if (component.name != player || objects.creature != entt::null)
		    {
			    return;
		    }
		    if (const auto* slot = registry.TryGet<const components::PlayerCreatureSlot>(entity);
		        slot != nullptr && slot->creature.has_value())
		    {
			    objects.creature = *slot->creature;
		    }
	    });
	// the towns in the order they joined the player's list
	std::vector<std::pair<uint32_t, entt::entity>> towns;
	registry.Each<const components::Town>([&towns, player](entt::entity entity, const components::Town& town) {
		if (town.owner == player)
		{
			towns.emplace_back(town.ownerListStamp, entity);
		}
	});
	std::ranges::sort(towns, [](const auto& left, const auto& right) {
		return left.first != right.first ? left.first < right.first
		                                 : entt::to_integral(left.second) < entt::to_integral(right.second);
	});
	for (const auto& [stamp, town] : towns)
	{
		objects.towns.push_back(town);
	}
	return objects;
}

std::optional<PlayerValues> saved_players::CurrentValues(PlayerNames player)
{
	if (!Locator::playerSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto& players = Locator::playerSystem::value();
	PlayerValues values;
	values.alignment = magic::players::AlignmentOf(player);
	const auto& magic = magic::players::ReadMagicOf(player);
	values.tribalPower = magic.tribalPower;
	values.maxTribalPower = magic.maxTribalPower;
	values.type = static_cast<uint32_t>(magic.playerType);
	values.allMagic = magic.allMagicCheat;
	values.magicEnabled = magic.remainder;
	values.everEnabled = magic.everEnabled;
	values.totalPopulation = magic::players::TotalPopulation(player);
	values.influencePower = players.InfluencePower(player);
	const auto rivalry = players.Rivalry(player);
	values.averageInfluencePower = rivalry.averageInfluencePower;
	values.damageFromPlayer = rivalry.harmFrom;
	values.allies = alliances::AlliancesOf(player).strength;
	return values;
}

void saved_players::Apply(PlayerNames player, const PlayerValues& values)
{
	if (!Locator::playerSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& players = Locator::playerSystem::value();
	magic::players::AlignmentOf(player) = values.alignment;
	auto& magic = magic::players::MagicOf(player);
	magic.tribalPower = values.tribalPower;
	magic.maxTribalPower = values.maxTribalPower;
	magic.playerType = static_cast<int32_t>(values.type);
	magic.allMagicCheat = values.allMagic;
	magic.remainder = values.magicEnabled;
	magic.everEnabled = values.everEnabled;
	magic::players::SetTotalPopulation(player, values.totalPopulation);
	players.SetInfluencePower(player, values.influencePower);
	auto rivalry = players.Rivalry(player);
	rivalry.averageInfluencePower = values.averageInfluencePower;
	rivalry.harmFrom = values.damageFromPlayer;
	players.SetRivalry(player, rivalry);
	alliances::Restore(player, components::PlayerAlliances {.strength = values.allies});
}

std::vector<ListedPlayer> saved_players::ListPlayers(const gamesave::WorldRead& world)
{
	std::vector<ListedPlayer> listed;
	if (world.objects.empty() || !world.objects.front().has_value())
	{
		return listed;
	}
	const auto* game = std::get_if<gamesave::GameHead>(&*world.objects.front());
	if (game == nullptr)
	{
		return listed;
	}
	for (const auto number : game->players)
	{
		if (number == 0 || number > world.objects.size() || !world.objects[number - 1].has_value())
		{
			continue;
		}
		const auto* record = std::get_if<gamesave::PlayerRecord>(&*world.objects[number - 1]);
		if (record != nullptr && record->playerNumber < k_Players)
		{
			listed.push_back({.number = number, .record = record});
		}
	}
	return listed;
}

size_t saved_players::Load(const gamesave::WorldRead& world, saved_world_write::LoadedObjects* loaded)
{
	const auto listed = ListPlayers(world);
	for (const auto& [number, record] : listed)
	{
		Apply(static_cast<PlayerNames>(record->playerNumber), ValuesOf(*record));
		if (loaded != nullptr)
		{
			loaded->insert_or_assign(number, saved_world_write::LoadedObject {.entity = entt::null,
			                                                                  .type = gamesave::k_PlayerType,
			                                                                  .id = record->playerNumber});
		}
	}
	return listed.size();
}
