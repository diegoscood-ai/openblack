/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Reactions.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <vector>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/ReactionRecords.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/MapCoords.h"
#include "ECS/Registry.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Objects/MapShield.h"

using namespace openblack;
using namespace openblack::ecs::effects;
using openblack::ecs::components::ReactionRecords;

namespace
{
std::vector<reactions::Reaction> g_Reactions;
uint32_t g_NextId = 1;
/// between BeginTurn and EndTurn the map cells were rebuilt at the start of the turn (Game::GameLogicLoop)
bool g_InTurn = false;
std::array<reactions::LivingReactionHandler, 3> g_Handlers {};

reactions::Reaction* FindMutable(uint32_t id)
{
	if (id == 0)
	{
		return nullptr;
	}
	const auto it =
	    std::find_if(g_Reactions.begin(), g_Reactions.end(), [id](const auto& reaction) { return reaction.id == id; });
	return it != g_Reactions.end() ? &*it : nullptr;
}

/// MapCoords::InBounds 0x6042C0 (0x6E3E7A): the cell's unsigned high words inside the map (the island's cells per side,
/// as ecs::sea_cells; out without a land)
bool InMap(const ecs::map_coords::MapCoords& coords)
{
	return Locator::terrainSystem::has_value() &&
	       ecs::map_coords::InBounds(coords, Locator::terrainSystem::value().GetCellsPerSide());
}

glm::vec2 PosOf(entt::entity entity)
{
	const auto& position = Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(entity).position;
	return {position.x, position.z};
}

/// Reaction::GetPos 0x6E45C0 = its initiator's GetPos (GameThingWithPos +0x14), as MapCoords in metres (x, z and the
/// altitude above the land in y). Most initiators are objects with a Transform (a world point); a Spell has no
/// Transform in openblack and keeps its own position (components::Spell +0x14), and a spell IS the initiator of the
/// shield reactions (REACTION 13 / 35 / 36, Magic/Spells/SpellShield) and of Spell +0x28. False: no position at all
bool MapPosOf(entt::entity entity, glm::vec3& out)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* transform = registry.TryGet<const ecs::components::Transform>(entity); transform != nullptr)
	{
		out = magic::ToMap(transform->position);
		return true;
	}
	if (const auto* spell = registry.TryGet<const ecs::components::Spell>(entity); spell != nullptr)
	{
		out = spell->position;
		return true;
	}
	return false;
}

/// The Living class of an object of a cell's list (-1: not a Living)
int ClassOf(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<ecs::components::Villager>(entity))
	{
		return static_cast<int>(reactions::LivingClass::Villager);
	}
	if (registry.AllOf<ecs::components::Animal>(entity))
	{
		return static_cast<int>(reactions::LivingClass::Animal);
	}
	return -1; // no creature yet
}
} // namespace

void reactions::SetLivingReactionHandler(LivingClass living, LivingReactionHandler handler)
{
	g_Handlers.at(static_cast<size_t>(living)) = handler;
}

uint32_t reactions::CreateReaction(entt::entity initiator, openblack::Reaction type, PlayerNames player, bool stamp)
{
	// (type & 0xFF) == -1 never holds for the byte, but the callers check reactionType != -1 first
	if (type == openblack::Reaction::None)
	{
		return 0;
	}
	Reaction reaction;
	reaction.id = g_NextId++;
	reaction.initiator = initiator;
	reaction.type = type;
	reaction.player = player;
	reaction.turnCreated = stamp ? game_clock::Turn() : 0;
	// Reaction::Reaction 0x6E39D0: +0x3C = whetherReactionGrows ? 1 : maxReactionDistance
	if (Locator::infoConstants::has_value())
	{
		const auto& table = Locator::infoConstants::value().reaction;
		if (const auto index = static_cast<size_t>(type); index < table.size())
		{
			reaction.radius = table[index].whetherReactionGrows != 0 ? 1.0f : table[index].maxReactionDistance;
		}
	}
	g_Reactions.push_back(reaction);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Reaction {}: type {} by entity {}, radius {:.1f}", reaction.id,
	                    static_cast<int>(type), static_cast<uint32_t>(initiator), reaction.radius);
	// SpreadReaction(GetPos, GetMapCellSpiralSizeFromRadius(GetRadius), type, initiator, this)
	SpreadReaction(reaction.id);
	return reaction.id;
}

void reactions::SpreadReaction(uint32_t id)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::entitiesMap::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto* found = Find(id);
	glm::vec3 initiator(0.0f);
	if (found == nullptr || !registry.Valid(found->initiator) || !MapPosOf(found->initiator, initiator))
	{
		return;
	}
	const Reaction reaction = *found;
	// outside the turn (the map script at load, the debug hooks) the cells are rebuilt first; inside it they are the
	// turn's (Game::GameLogicLoop rebuilds them before the Living)
	if (!g_InTurn)
	{
		Locator::entitiesMap::value().Rebuild();
	}
	const glm::vec2 at(initiator.x, initiator.z); // MapPosOf: a Spell initiator has no Transform (Reaction::GetPos 0x6E45C0)
	const int cells = ecs::map_coords::CellSpiralSize(reaction.radius); // GetMapCellSpiralSizeFromRadius 0x74F520
	const auto atCoords = ecs::map_coords::FromMetres(at);
	auto coords = atCoords;
	ecs::map_coords::Spiral spiral; // GUtils::Spiral 0x74D7E0, dir = count = 1 (0x6E3E51..0x6E3E5E)
	for (int i = 0; i < cells; ++i)
	{
		// 0x6E3E91 fn_0074CD50 = GUtils::GetDistanceInMetres 0x74CD70 from the reaction to the cell, then the radius is
		// compared against it (fcomp; test ah, 1 at 0x6E3EA4: the cell is kept while radius >= d)
		if (InMap(coords) && gutils::GetDistanceInMetres(atCoords, coords) <= reaction.radius)
		{
			// the cell's mobile list (+0, 0x6E3EEA / 0x6E3FDB), from its head (ecs::map_cells): every Living of it,
			// whatever its class, in that order
			const auto livings = ecs::map_cells::MobileInCell(ecs::map_coords::Cell(coords));
			for (const auto entity : livings)
			{
				// the handlers may create or remove reactions: this one is looked up again for each Living
				const auto* current = Find(reaction.id);
				if (current == nullptr || entity == reaction.initiator || !registry.Valid(entity) ||
				    !registry.AllOf<components::Transform>(entity))
				{
					continue;
				}
				const int living = ClassOf(entity);
				if (living < 0 || g_Handlers.at(static_cast<size_t>(living)) == nullptr)
				{
					continue;
				}
				// 0x6E4031 fn_0072B990 (after the class's vt+0x984 test, before the distance): a Living under a shield the
				// reaction's source is not definitely inside ignores it, villagers and animals alike
				if (magic::map_shield::IsReactionBlockedByShield(
				        magic::ToMap(registry.Get<const components::Transform>(entity).position), initiator))
				{
					continue;
				}
				const glm::vec2 p = PosOf(entity);
				const float d = (std::abs(p.y - at.y) + std::abs(p.x - at.x)) * 0.5f;
				const Reaction copy = *current;
				g_Handlers.at(static_cast<size_t>(living))(entity, copy, d);
			}
		}
		ecs::map_coords::AddCells(coords, spiral.Next()); // MapCoords += JustMapXZ 0x605470 (0x6E3F6E): the fraction stays
	}
}

void reactions::RemoveAllReactionsInitiatedByObject(entt::entity initiator)
{
	std::erase_if(g_Reactions, [initiator](const Reaction& reaction) { return reaction.initiator == initiator; });
}

void reactions::RemoveAllReactionsOfTypeInitiatedBy(entt::entity initiator, openblack::Reaction type)
{
	std::erase_if(g_Reactions, [initiator, type](const Reaction& reaction) {
		return reaction.initiator == initiator && reaction.type == type;
	});
}

void reactions::Prune()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	std::erase_if(g_Reactions, [&registry](const Reaction& reaction) { return !registry.Valid(reaction.initiator); });
}

void reactions::Stamp(uint32_t reaction)
{
	if (auto* entry = FindMutable(reaction); entry != nullptr)
	{
		entry->turnCreated = game_clock::Turn();
	}
}

void reactions::MarkStarted(uint32_t reaction, uint32_t turn)
{
	if (auto* entry = FindMutable(reaction); entry != nullptr && entry->turnCreated == 0)
	{
		entry->turnCreated = turn;
	}
}

void reactions::SetRadius(uint32_t reaction, float radius)
{
	if (auto* entry = FindMutable(reaction); entry != nullptr)
	{
		entry->radius = radius;
	}
}

void reactions::SetInitiator(uint32_t reaction, entt::entity initiator)
{
	if (auto* entry = FindMutable(reaction); entry != nullptr)
	{
		entry->initiator = initiator;
	}
}

uint32_t reactions::GetReactionInitiatedBy(entt::entity initiator)
{
	const auto it = std::find_if(g_Reactions.begin(), g_Reactions.end(),
	                             [initiator](const Reaction& reaction) { return reaction.initiator == initiator; });
	return it == g_Reactions.end() ? 0 : it->id;
}

uint32_t reactions::GetReactionOfTypeInitiatedBy(entt::entity initiator, openblack::Reaction type)
{
	const auto it = std::find_if(g_Reactions.begin(), g_Reactions.end(), [initiator, type](const Reaction& reaction) {
		return reaction.initiator == initiator && reaction.type == type && reaction.available;
	});
	return it == g_Reactions.end() ? 0 : it->id;
}

const std::vector<reactions::Reaction>& reactions::All()
{
	return g_Reactions;
}

const reactions::Reaction* reactions::Find(uint32_t id)
{
	return FindMutable(id);
}

bool reactions::IsAvailable(const Reaction* reaction)
{
	return reaction != nullptr && reaction->available && Locator::entitiesRegistry::value().Valid(reaction->initiator);
}

bool reactions::Records(entt::entity living, uint8_t type, uint32_t again, uint32_t now)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& memory = registry.AllOf<ReactionRecords>(living) ? registry.Get<ReactionRecords>(living)
	                                                        : registry.Assign<ReactionRecords>(living);
	for (uint8_t i = 0; i < memory.count;)
	{
		auto& record = memory.records[i];
		if (record.type == type)
		{
			if (now - record.turn > again)
			{
				record.turn = now;
				return true;
			}
			return false;
		}
		if (now - record.turn > 1800)
		{
			std::copy(memory.records.begin() + i + 1, memory.records.begin() + memory.count, memory.records.begin() + i);
			--memory.count;
			continue;
		}
		++i;
	}
	if (memory.count >= memory.records.size())
	{
		std::copy(memory.records.begin() + 1, memory.records.end(), memory.records.begin());
		--memory.count;
	}
	memory.records[memory.count++] = {type, now};
	return true;
}

uint32_t reactions::RecordTurn(entt::entity living, uint8_t type)
{
	const auto* memory = Locator::entitiesRegistry::value().TryGet<const ReactionRecords>(living);
	if (memory == nullptr)
	{
		return 0;
	}
	for (uint8_t i = 0; i < memory->count; ++i)
	{
		if (memory->records[i].type == type)
		{
			return memory->records[i].turn;
		}
	}
	return 0;
}

void reactions::RefreshRecord(entt::entity living, uint8_t type, uint32_t now)
{
	auto* memory = Locator::entitiesRegistry::value().TryGet<ReactionRecords>(living);
	if (memory == nullptr)
	{
		return;
	}
	for (uint8_t i = 0; i < memory->count; ++i)
	{
		if (memory->records[i].type == type)
		{
			memory->records[i].turn = now;
		}
	}
}

uint32_t reactions::Score(uint8_t type, bool reactsToType, uint32_t priority, float distance)
{
	const auto& reaction = Locator::infoConstants::value().reaction.at(type);
	if (!reactsToType || distance > reaction.maxReactionDistance)
	{
		return 0;
	}
	const float max = reaction.maxReactionDistance;
	// std::max(max, 0.0001f): openblack's divide-by-zero guard, not in 0x6E4620
	const float score = static_cast<float>(priority) *
	                    (1.0f + 0.5f * reaction.howImportantIsDistance * (max - distance) / std::max(max, 0.0001f));
	return static_cast<uint32_t>(std::trunc(std::min(255.0f, score)));
}

bool reactions::MaySwitch(float currentScore, float newScore, float seconds, uint8_t currentType)
{
	constexpr uint8_t k_ReactToHandPickUp = 16;
	if (!(currentScore < newScore))
	{
		return false;
	}
	const float h = std::max(10.0f, currentScore / newScore * 20.0f - 10.0f);
	return !(seconds < h && (seconds < 1.0f || currentType != k_ReactToHandPickUp));
}

uint32_t reactions::Turn()
{
	return game_clock::Turn();
}

void reactions::BeginTurn()
{
	g_InTurn = true;
	Prune();
}

void reactions::EndTurn()
{
	g_InTurn = false;
}

void reactions::Clear()
{
	g_Reactions.clear();
	g_NextId = 1;
	g_InTurn = false;
}
