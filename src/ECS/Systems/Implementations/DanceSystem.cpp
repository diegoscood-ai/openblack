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
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <DanceFile.h>
#include <DanceShapeFile.h>
#include <entt/core/hashed_string.hpp>

#include "ECS/Components/Dance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Villager.h"
#include "ECS/DanceMoves.h"
#include "ECS/DanceRules.h"
#include "ECS/DanceShapes.h"
#include "ECS/Dances.h"
#include "ECS/Footpaths.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownQueries.h"
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
                                 bool madeByScript, entt::entity town)
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
	const auto dance = dances::Create(Locator::entitiesRegistry::value(),
	                                  {.type = type,
	                                   .autostart = info.startsAutomatically != 0,
	                                   .place = place,
	                                   .owner = owner,
	                                   .duration = duration,
	                                   .madeByScript = madeByScript},
	                                  std::move(file));
	// Then a footpath to it from the storage pit of the town it is made for, or of the town nearest it however far: not
	// for the worship dance nor the worship sites' dances
	if (dance_rules::LaysTownFootpath(type))
	{
		if (town == entt::null)
		{
			town = map_cells::GetNearestTown(place, std::numeric_limits<float>::max());
		}
		if (const auto pit = town != entt::null ? town_queries::GetStoragePit(town) : entt::null; pit != entt::null)
		{
			static_cast<void>(footpaths::GenerateRoutesBetween(pit, dance));
		}
	}
	return dance;
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

const std::array<ecs::dance_shapes::Shape, ecs::dance_shapes::k_ShapeCount>& DanceSystem::Shapes()
{
	if (_shapes.has_value())
	{
		return *_shapes;
	}
	_shapes = ecs::dance_moves::Shapes();
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return *_shapes;
	}
	// The shapes from files are read from the data folder, one file a shape; one that can't be read keeps its points at
	// the centre
	auto& cache = Locator::resources::value().GetDanceShapes();
	for (std::size_t i = ecs::dance_shapes::k_FirstFileShape; i < _shapes->size(); ++i)
	{
		const std::string name = ecs::dance_shapes::FileName(i);
		const auto id = entt::hashed_string(("dance-shape/" + name).c_str()).value();
		if (!cache.Contains(id))
		{
			cache.Load(id, resources::DanceShapeLoader::FromDiskTag {},
			           Locator::filesystem::value().GetPath<filesystem::Path::Data>() / (name + ".DAN"));
		}
		const auto file = cache.Handle(id).handle();
		if (!file)
		{
			continue;
		}
		std::vector<glm::ivec2> points;
		points.reserve(file->points.size());
		std::ranges::transform(file->points, std::back_inserter(points),
		                       [](const dance::DanceShapePoint& point) { return glm::ivec2(point.x, point.z); });
		_shapes->at(i) = ecs::dance_shapes::FromPoints(points);
	}
	return *_shapes;
}
