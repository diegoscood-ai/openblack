/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureWebPageWriter.h"

#include <algorithm>
#include <span>
#include <string>
#include <utility>

#include <spdlog/spdlog.h>

#include "Creature/CreatureDesires.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "Creature/CreatureSpells.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/CreatureTallies.h"
#include "ECS/CreatureNaming.h"
#include "ECS/Life.h"
#include "ECS/Registry.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::creature_web_page_writer
{

using namespace components;

std::optional<creature_web_page::Facts> FactsOf(const Registry& registry, const InfoConstants* info, entt::entity creature)
{
	const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	if (body == nullptr)
	{
		return std::nullopt;
	}
	creature_web_page::Facts facts;
	facts.name = creature_naming::NameOf(registry, creature);
	facts.health = life::LifeOf(creature);
	facts.alignment = body->alignment;
	facts.strength = body->strength;
	facts.fatness = body->fatness;
	if (const auto* needs = registry.TryGet<const CreatureNeeds>(creature))
	{
		facts.age = needs->needs.age;
		facts.energy = needs->needs.energy;
		facts.exhaustion = needs->needs.exhaustion;
		facts.dehydration = needs->needs.dehydration;
		facts.itchiness = needs->needs.itchiness;
		facts.poo = needs->needs.poo;
		facts.turnsLived = needs->needs.turns;
	}
	// what it counts of its life: the kills by what it ate, its fights and wins, its poos and the mushrooms it ate
	if (const auto* kept = registry.TryGet<const CreatureTallies>(creature))
	{
		facts.tallies.at(0) = kept->meals.peopleKilled;
		facts.tallies.at(1) = kept->meals.animalsKilled;
		facts.tallies.at(2) = kept->meals.creaturesKilled;
		facts.tallies.at(5) = kept->poos;
		facts.tallies.at(6) = kept->mushroomsEaten;
	}
	if (const auto* record = registry.TryGet<const CreatureFightRecord>(creature))
	{
		facts.tallies.at(3) = record->fights;
		facts.tallies.at(4) = record->wins;
	}
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	// each desire by the name its row of the tables gives the page, at the level it is felt (none felt before the mind
	// has set its desires up)
	for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
	{
		creature_web_page::Facts::Desire desire;
		if (info != nullptr && d < info->creatureInitialDesire.size())
		{
			const auto& name = info->creatureInitialDesire.at(d).field0xf0;
			desire.name.assign(name.begin(), std::ranges::find(name, '\0'));
		}
		if (mind != nullptr && mind->desires.has_value())
		{
			desire.level = mind->desires->desires.at(d).value;
		}
		facts.desires.push_back(std::move(desire));
	}
	if (mind != nullptr)
	{
		for (uint32_t kind = 0; kind < facts.beliefs.size(); ++kind)
		{
			facts.beliefs.at(kind) = static_cast<uint32_t>(mind->beliefs.Count(kind));
		}
		facts.attitudeToPlayer = mind->attitudeToPlayer;
		facts.turnsLookedAt = mind->turnsLookedAt;
	}
	if (const auto* spells = registry.TryGet<const CreatureSpells>(creature))
	{
		const auto& slots = spells->spells.slots;
		const auto on = std::ranges::find_if(slots, [](const auto& slot) { return slot.phase != creature_spells::Phase::Off; });
		if (on != slots.end())
		{
			facts.spellReceived = static_cast<uint32_t>(std::distance(slots.begin(), on));
		}
	}
	return facts;
}

bool Write(entt::entity creature)
{
	if (!Locator::playerProfileSystem::has_value() || !Locator::filesystem::has_value() || !Locator::resources::has_value() ||
	    !Locator::entitiesRegistry::has_value())
	{
		return false;
	}
	auto& profiles = Locator::playerProfileSystem::value();
	const auto folder = profiles.CurrentPlayerFolder();
	if (!folder.has_value())
	{
		return false;
	}
	auto& fileSystem = Locator::filesystem::value();
	// the template is looked for first: without it nothing is written, not even the folder
	const auto templatePath = fileSystem.GetPath<filesystem::Path::Scripts>() / creature_web_page::k_TemplateFile;
	if (!fileSystem.Exists(templatePath))
	{
		return false;
	}
	const auto* info = Locator::infoConstants::has_value() ? &Locator::infoConstants::value() : nullptr;
	const auto facts = FactsOf(std::as_const(Locator::entitiesRegistry::value()), info, creature);
	if (!facts.has_value())
	{
		return false;
	}
	const auto& bytes = resources::LoadOptionalBlob(Locator::resources::value().GetBlobs(), templatePath, "creature page");
	const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	const auto page = creature_web_page::Fill(text, creature_web_page::FieldsOf(*facts));
	if (!fileSystem.MakeFolder(*folder / creature_web_page::k_Folder))
	{
		return false;
	}
	const auto path = creature_web_page::PagePath(facts->name);
	const bool written = profiles.WritePlayerFile(path, std::span(reinterpret_cast<const uint8_t*>(page.data()), page.size()));
	if (written)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "The creature's page is written to {}", (*folder / path).generic_string());
	}
	return written;
}

void ProcessTurn()
{
	if (!Locator::playerSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto& players = Locator::playerSystem::value();
	const auto creature = players.GetPrimaryCreature(players.GetLocalPlayer());
	if (!creature.has_value())
	{
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* needs = registry.Valid(*creature) ? registry.TryGet<const CreatureNeeds>(*creature) : nullptr;
	if (needs != nullptr && creature_web_page::IsSaveTurn(needs->needs.turns))
	{
		Write(*creature);
	}
}

} // namespace openblack::ecs::creature_web_page_writer
