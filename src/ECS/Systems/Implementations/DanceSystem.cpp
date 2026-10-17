/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "DanceSystem.h"

#include <cctype>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <DanceFile.h>
#include <entt/core/hashed_string.hpp>

#include "ECS/Components/Dance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Villager.h"
#include "ECS/Dances.h"
#include "ECS/Registry.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerCore.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "VillagerDance.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{

/// Where a dance's file is: its name in the table is under the game's scripts folder
std::filesystem::path DanceFilePath(const std::string& name)
{
	// The table's names separate their folders with one or two backslashes
	std::vector<std::string> parts;
	std::string part;
	for (const char c : name)
	{
		if (c == '\\' || c == '/')
		{
			if (!part.empty())
			{
				parts.push_back(std::move(part));
				part.clear();
			}
			continue;
		}
		part.push_back(c);
	}
	if (!part.empty())
	{
		parts.push_back(std::move(part));
	}
	const auto lower = [](std::string text) {
		std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return text;
	};
	auto path = Locator::filesystem::value().GetPath<filesystem::Path::Scripts>();
	for (std::size_t i = 0; i < parts.size(); ++i)
	{
		if (i == 0 && lower(parts[i]) == "scripts")
		{
			continue;
		}
		path /= parts[i];
	}
	return path;
}

/// A dance's file, read once through the resource cache; none when it can't be opened
std::shared_ptr<const dance::DanceFile> LoadDanceFile(const std::string& name)
{
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return nullptr;
	}
	auto& cache = Locator::resources::value().GetDanceFiles();
	const auto id = entt::hashed_string(name.c_str()).value();
	if (!cache.Contains(id))
	{
		cache.Load(id, resources::DanceFileLoader::FromDiskTag {}, DanceFilePath(name));
	}
	return cache.Handle(id).handle();
}

/// A villager that has finished dancing is told to decide what to do next (a dancer in the scripts' dance state
/// refuses, and stays as it is); anything else is told nothing
void FinishedDancing(entt::entity living)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.AllOf<Villager, LivingAction>(living))
	{
		return;
	}
	static_cast<void>(ecs::villager::SetTopState(living, VillagerStates::DecideWhatToDo));
}

/// Whether a thing a dance is danced about is still about: not marked to go, and a villager not dying by its final
/// state
bool Available(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	const bool going = !ecs::IsAvailable(thing);
	if (going || !registry.AllOf<Villager, LivingAction>(thing))
	{
		return ecs::dances::IsStillAbout(going, std::nullopt);
	}
	return ecs::dances::IsStillAbout(going, ecs::villager::GetFinalState(thing));
}
} // namespace

entt::entity DanceSystem::Create(DanceInfo type, const map_coords::MapCoords& place, entt::entity owner, uint32_t duration,
                                 bool madeByScript)
{
	if (!Locator::infoConstants::has_value())
	{
		return entt::null;
	}
	const auto& table = Locator::infoConstants::value().dance;
	const auto row = static_cast<std::size_t>(type);
	// TODO(dance): the game reads any row it is given, past the table's end too, and makes the dance from whatever it
	// finds there; a row outside the table makes no dance here
	if (row >= table.size())
	{
		return entt::null;
	}
	const auto& info = table.at(row);
	const std::string_view field(info.fileName.data(), info.fileName.size());
	const std::string fileName(field.substr(0, field.find('\0')));
	// A file that can't be opened still makes the dance: one group, no key frames
	auto file = LoadDanceFile(fileName);
	// TODO(opening): a town's dance makes footpaths to it from the town's storage pit
	return dances::Create(Locator::entitiesRegistry::value(),
	                      {.type = type,
	                       .autostart = info.startsAutomatically != 0,
	                       .place = place,
	                       .owner = owner,
	                       .duration = duration,
	                       .madeByScript = madeByScript},
	                      std::move(file));
}

void DanceSystem::Destroy(entt::entity dance)
{
	dances::Destroy(Locator::entitiesRegistry::value(), dance, &FinishedDancing);
}

void DanceSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	// The newest first; a dance gone in its turn is passed over, and one made in a turn waits for the next
	const auto all = ecs::dances::InTurnOrder(registry);
	const dances::TurnContext context {.turn = Locator::time::value().GetTurn(),
	                                   .available = &Available,
	                                   .finished = &FinishedDancing,
	                                   .playClipAgain = &ecs::villager_dance::PlayClipAgain};
	for (const auto entity : all)
	{
		if (dances::IsDance(registry, entity))
		{
			dances::ProcessTurn(registry, entity, context);
		}
	}
}
