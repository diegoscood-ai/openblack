/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AnimationSounds.h"

#include <cstdlib>
#include <cstring>

#include <array>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <LNDFile.h>
#include <PackFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DAnim.h"
#include "3D/LandIslandInterface.h"
#include "Audio/AudioManagerInterface.h"
#include "Camera/Camera.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Animations.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::audio
{
namespace
{
constexpr int32_t k_Wildcard = 0x0FFF0000;
constexpr int32_t k_ClipCount = 441;

struct Event
{
	int32_t time;
	int32_t soundId;
	int32_t action; ///< 0 play, 1 stop (only M_P_Saw_Wood)
};

struct ClipSounds
{
	int32_t group {0};
	std::vector<Event> events;
};

struct Tables
{
	bool loaded {false};
	std::unordered_map<int32_t, ClipSounds> clips;
	std::vector<std::array<int32_t, 6>> rows; ///< LHAudioAnimArrayTable: voice, ?, group, surface, soundId, list index
	std::vector<int32_t> waves;               ///< LHAudioWaveNumTable: lists {count, sample numbers...}
};

Tables& Load()
{
	static Tables tables;
	if (tables.loaded)
	{
		return tables;
	}
	tables.loaded = true;
	auto& fileSystem = Locator::filesystem::value();
	auto& animations = Locator::resources::value().GetAnimations();
	std::unordered_map<std::string, int32_t> byName;
	for (int32_t i = 0; i < k_ClipCount; ++i)
	{
		if (animations.Contains(ecs::ClipId(static_cast<uint32_t>(i))))
		{
			const auto& name = animations.Handle(ecs::ClipId(static_cast<uint32_t>(i)))->GetName();
			byName.emplace(std::string(name.c_str()), i); // the header's name is 32 chars padded with zeros
		}
	}
	try
	{
		// LoadAllAnimations 0x550180: "hasGroup", then "name [group]" and "time soundId action more" lines until more
		// is 0, until END
		const auto text = fileSystem.ReadAll(fileSystem.GetPath<filesystem::Path::Data>() / "SmallSounds.SAS");
		std::istringstream in(std::string(text.begin(), text.end()));
		int hasGroup = 0;
		in >> hasGroup;
		std::string name;
		while (in >> name && name != "END")
		{
			ClipSounds clip;
			if (hasGroup != 0)
			{
				in >> clip.group;
			}
			int more = 1;
			while (more != 0)
			{
				Event event {};
				if (!(in >> event.time >> event.soundId >> event.action >> more))
				{
					more = 0;
					break;
				}
				clip.events.push_back(event);
			}
			if (const auto index = byName.find(name); index != byName.end())
			{
				tables.clips[index->second] = std::move(clip);
			}
		}
		// editor.sad's animation effect tables
		pack::PackFile bank;
		if (bank.ReadFile(*fileSystem.GetData(fileSystem.GetPath<filesystem::Path::Audio>() / "Sfx" / "Game" / "editor.sad")) ==
		    pack::PackResult::Success)
		{
			const auto& blocks = bank.GetBlocks();
			const auto rows = blocks.find("LHAudioAnimArrayTable");
			const auto waves = blocks.find("LHAudioWaveNumTable");
			if (rows != blocks.end() && waves != blocks.end() && rows->second.size() >= 8)
			{
				int32_t count = 0;
				int32_t width = 0;
				std::memcpy(&count, rows->second.data(), 4);
				std::memcpy(&width, rows->second.data() + 4, 4);
				for (int32_t r = 0; width == 6 && r < count && 8 + (r + 1) * 24 <= static_cast<int32_t>(rows->second.size()); ++r)
				{
					std::array<int32_t, 6> row {};
					std::memcpy(row.data(), rows->second.data() + 8 + r * 24, 24);
					tables.rows.push_back(row);
				}
				tables.waves.resize(waves->second.size() / 4);
				std::memcpy(tables.waves.data(), waves->second.data(), tables.waves.size() * 4);
			}
		}
	}
	catch (const std::exception& error)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("audio"), "Animation sounds: {}", error.what());
	}
	SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Animation sounds: {} clips, {} effect rows", tables.clips.size(), tables.rows.size());
	return tables;
}

/// GSoundMap::GetSurfaceType (0x71D8E0): 7 under water, else the surfaceSound of the cell's material (info.dat)
int32_t SurfaceType(glm::vec3 position)
{
	if (!Locator::terrainSystem::has_value())
	{
		return 3;
	}
	auto& island = Locator::terrainSystem::value();
	if (island.GetHeightAt(glm::vec2(position.x, position.z)) <= 0.0f)
	{
		return 7;
	}
	const auto& countries = island.GetCountries();
	const auto& materials = island.GetMaterialInfo();
	const int last = island.GetCellsPerSide() - 1;
	const auto cellCoordinates = glm::clamp(glm::ivec2(position.x / 10.0f, position.z / 10.0f), 0, last);
	const auto& cell = island.GetCell(glm::u16vec2(cellCoordinates));
	if (cell.properties.country >= countries.size())
	{
		return 3;
	}
	// country + altitude * 12 + 8: the second material of the cell's altitude
	const auto& country = countries[cell.properties.country];
	const auto altitude = std::min<uint16_t>(island.GetCellAltitude(cell), 255);
	const auto material = country.materials[altitude].indices[1];
	if (material >= materials.size())
	{
		return 3;
	}
	const auto& info = Locator::infoConstants::value().terrainMaterial;
	const auto type = materials[material].type;
	const auto surface = type < info.size() ? static_cast<int32_t>(info[type].surfaceSound) : 3;
	return surface >= 1 && surface <= 8 ? surface : 3;
}

/// LHFindAttribRow (LHaudiodllR 0x10014420): the matching row with the most exact columns, the later one on a tie
int32_t PickSample(const Tables& tables, const std::array<int32_t, 5>& key)
{
	int best = -1;
	size_t bestRow = 0;
	for (size_t r = 0; r < tables.rows.size(); ++r)
	{
		int exact = 0;
		bool match = true;
		for (size_t c = 0; c < 5 && match; ++c)
		{
			const auto value = tables.rows[r][c];
			if (value == k_Wildcard)
			{
				continue;
			}
			match = value == key.at(c);
			++exact;
		}
		if (match && exact >= best)
		{
			best = exact;
			bestRow = r;
		}
	}
	if (best < 0)
	{
		return 0;
	}
	const auto list = static_cast<size_t>(tables.rows[bestRow][5]);
	if (list >= tables.waves.size() || tables.waves[list] <= 0 || list + static_cast<size_t>(tables.waves[list]) >= tables.waves.size())
	{
		return 0;
	}
	const auto count = tables.waves[list];
	const auto pick = count == 1 ? 0 : Locator::rng::value().NextValue<int32_t>(0, count - 1);
	return tables.waves[list + 1 + static_cast<size_t>(pick)];
}
} // namespace

void AnimationSounds::Fire(entt::entity entity, int32_t clip, int32_t from, int32_t to)
{
	if (!Locator::audio::has_value() || !Locator::camera::has_value() || from >= to)
	{
		return;
	}
	const auto& tables = Load();
	const auto sounds = tables.clips.find(clip);
	if (sounds == tables.clips.end())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const ecs::components::Transform>(entity);
	if (transform == nullptr)
	{
		return;
	}
	const auto* villager = registry.TryGet<const ecs::components::Villager>(entity);
	const auto* action = registry.TryGet<const ecs::components::LivingAction>(entity);
	for (const auto& event : sounds->second.events)
	{
		if (event.time < from || event.time >= to || event.action != 0)
		{
			continue; // stopping a sample (the saw) is not done: the samples are short
		}
		int32_t voice = 2;
		if (sounds->second.group == 1)
		{
			if (villager != nullptr && villager->health == 0)
			{
				return; // not alive
			}
			voice = villager == nullptr || villager->lifeStage == ecs::components::Villager::LifeStage::Child ? 3
			        : villager->sex == ecs::components::Villager::Sex::FEMALE                              ? 2
			                                                                                                : 1;
		}
		// the banter of VillagersBanter.sad (0x92-0x94) is not played yet
		if (event.soundId >= 0x92 && event.soundId <= 0x94)
		{
			continue;
		}
		// the thrown screams only just after being thrown
		const uint16_t turns = action != nullptr ? action->turnsSinceStateChange : 0;
		if ((clip == 399 && (villager == nullptr || turns >= 15)) || (clip == 401 && (villager == nullptr || turns >= 10)))
		{
			continue;
		}
		const std::array<int32_t, 5> key = {voice, 2, sounds->second.group, SurfaceType(transform->position), event.soundId};
		const auto sample = PickSample(tables, key);
		if (sample <= 0)
		{
			continue;
		}
		const auto id = entt::hashed_string(fmt::format("editor.sad/{}", sample).c_str()).value();
		if (!Locator::resources::value().GetSounds().Contains(id))
		{
			continue;
		}
		auto& audio = Locator::audio::value();
		const auto& sound = audio.GetSound(id);
		// LHSamplePlay: not beyond the sample's max distance from the camera (NULL.wav fillers have 0: never)
		const auto distance = glm::distance(transform->position, Locator::camera::value().GetOrigin());
		if (distance > sound.maxDistance)
		{
			continue;
		}
		const auto emitter = audio.CreateEmitter(id, PlayType::Once, transform->position, glm::vec3(0.0f), glm::vec2(0.0f),
		                                         sound.volume, AudioStatus::Playing, false);
		registry.Get<ecs::components::Transform>(emitter).position = transform->position;
		audio.PlayEmitter(emitter);
		if (std::getenv("OPENBLACK_ANIM_TRACE") != nullptr)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Animation sound: clip {} at {} ms, id {} surface {} -> editor.sad/{} ({})",
			                   clip, event.time, event.soundId, key[3], sample, sound.name);
		}
	}
}

} // namespace openblack::audio
