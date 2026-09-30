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

#include <algorithm>
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
#include "Audio/AnimEffectBank.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/SoundMap.h"
#include "Camera/Camera.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Animations.h"
#include "ECS/Components/AudioEmitter.h"
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
constexpr int32_t k_ClipCount = 441;

struct Event
{
	int32_t time;
	int32_t soundId;
	int32_t action; ///< 0 play, 1 stop (only M_P_Saw_Wood: forward saw off, back saw on)
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
	AnimEffectBank editor {"editor.sad", {}, {}, {}};
	AnimEffectBank banter {"VillagersBanter.sad", {}, {}, {}}; ///< soundIds 0x92-0x94
};

/// A playing sample that follows its object (the game's 3D callback 0x427200, Get3DSoundPos)
struct Playing
{
	entt::entity emitter;
	entt::entity owner;
	entt::id_type sound;
};
std::vector<Playing> g_Playing;

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
					break;
				}
				clip.events.push_back(event);
			}
			if (const auto index = byName.find(name); index != byName.end())
			{
				tables.clips[index->second] = std::move(clip);
			}
		}
		const auto audio = fileSystem.GetPath<filesystem::Path::Audio>();
		tables.editor.Load(audio / "Sfx" / "Game" / "editor.sad");
		tables.banter.Load(audio / "Dialogue" / "VillagersBanter.sad");
	}
	catch (const std::exception& error)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("audio"), "Animation sounds: {}", error.what());
	}
	SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Animation sounds: {} clips, {} + {} effect rows", tables.clips.size(),
	                   tables.editor.rows.size(), tables.banter.rows.size());
	return tables;
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
	if (registry.TryGet<const ecs::components::Transform>(entity) == nullptr)
	{
		return;
	}
	const auto* villager = registry.TryGet<const ecs::components::Villager>(entity);
	const auto* action = registry.TryGet<const ecs::components::LivingAction>(entity);
	auto& audio = Locator::audio::value();
	for (const auto& event : sounds->second.events)
	{
		if (event.time < from || event.time >= to)
		{
			continue;
		}
		int32_t voice = 2;
		if (sounds->second.group == 1)
		{
			if (villager != nullptr && villager->life <= 0.0f)
			{
				return; // not alive: the whole list is dropped
			}
			voice = villager == nullptr || villager->lifeStage == ecs::components::Villager::LifeStage::Child ? 3
			        : villager->sex == ecs::components::Villager::Sex::FEMALE                              ? 2
			                                                                                                : 1;
		}
		// fn_00516510's cases: banter from VillagersBanter.sad (0x92 heard at the villager's house), the thrown
		// screams only just after being thrown
		auto owner = entity;
		const AnimEffectBank* bank = &tables.editor;
		if (event.soundId >= 0x92 && event.soundId <= 0x94)
		{
			bank = &tables.banter;
			if (event.soundId == 0x92)
			{
				if (villager == nullptr || villager->abode == entt::null || !registry.Valid(villager->abode) ||
				    !registry.AllOf<ecs::components::Transform>(villager->abode))
				{
					continue;
				}
				owner = villager->abode;
			}
		}
		else
		{
			const uint16_t turns = action != nullptr ? action->turnsSinceStateChange : 0;
			if ((clip == 399 && (villager == nullptr || turns >= 15)) || (clip == 401 && (villager == nullptr || turns >= 10)))
			{
				continue;
			}
		}
		const auto& position = registry.Get<const ecs::components::Transform>(owner).position;
		const std::array<int32_t, 5> key = {voice, 2, sounds->second.group, GetSurfaceType(position), event.soundId};
		const auto list = bank->FindList(key);
		const bool trace = std::getenv("OPENBLACK_ANIM_TRACE") != nullptr;
		if (list.empty())
		{
			if (trace)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Animation sound: clip {} id {} key {},{},{},{}: no row", clip,
				                   event.soundId, key[0], key[2], key[3], key[4]);
			}
			continue;
		}
		if (event.action != 0)
		{
			// LHSampleStop of every sample of the list playing for that object
			for (auto& playing : g_Playing)
			{
				if (playing.owner == owner && registry.Valid(playing.emitter) &&
				    std::ranges::any_of(list, [&](int32_t sample) { return bank->SoundId(sample) == playing.sound; }))
				{
					audio.StopEmitter(playing.emitter);
				}
			}
			continue;
		}
		const auto sample = list.size() == 1 ? list[0] : list[Locator::rng::value().NextValue<size_t>(0, list.size() - 1)];
		const auto id = bank->SoundId(sample);
		if (sample <= 0 || !Locator::resources::value().GetSounds().Contains(id))
		{
			continue;
		}
		const auto& sound = audio.GetSound(id);
		// LHSamplePlay: not beyond the sample's max distance from the camera (NULL.wav fillers have 0: never)
		if (glm::distance(position, Locator::camera::value().GetOrigin()) > sound.maxDistance)
		{
			if (trace && bank == &tables.banter)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Animation sound: banter {} too far ({:.1f} > {})", sample,
				                   glm::distance(position, Locator::camera::value().GetOrigin()), sound.maxDistance);
			}
			continue;
		}
		const auto emitter = audio.CreateEmitter(id, PlayType::Once, position, glm::vec3(0.0f), glm::vec2(0.0f), sound.volume,
		                                         AudioStatus::Playing, false);
		registry.Get<ecs::components::Transform>(emitter).position = position;
		audio.PlayEmitter(emitter);
		g_Playing.push_back({emitter, owner, id});
		if (trace)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Animation sound: clip {} at {} ms, id {} surface {} -> {}/{} ({})", clip,
			                   event.time, event.soundId, key[3], bank->name, sample, sound.name);
		}
	}
}

void AnimationSounds::PlayFromTable(entt::entity owner, glm::vec3 position, const std::array<int32_t, 5>& key)
{
	if (!Locator::audio::has_value() || !Locator::camera::has_value())
	{
		return;
	}
	const auto& bank = Load().editor;
	const auto list = FindList(bank, key);
	if (list.empty())
	{
		return;
	}
	const auto sample = list.size() == 1 ? list[0] : list[Locator::rng::value().NextValue<size_t>(0, list.size() - 1)];
	const auto id = SampleSoundId(bank, sample);
	if (sample <= 0 || !Locator::resources::value().GetSounds().Contains(id))
	{
		return;
	}
	auto& audio = Locator::audio::value();
	const auto& sound = audio.GetSound(id);
	// LHSamplePlay: not beyond the sample's max distance from the camera
	if (glm::distance(position, Locator::camera::value().GetOrigin()) > sound.maxDistance)
	{
		return;
	}
	const auto emitter = audio.CreateEmitter(id, PlayType::Once, position, glm::vec3(0.0f), glm::vec2(0.0f), sound.volume,
	                                         AudioStatus::Playing, false);
	Locator::entitiesRegistry::value().Get<ecs::components::Transform>(emitter).position = position;
	audio.PlayEmitter(emitter);
	g_Playing.push_back({emitter, owner, id});
	if (std::getenv("OPENBLACK_ANIM_TRACE") != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Animation sound: key {},{},{},{},{} -> editor.sad/{} ({})", key[0], key[1],
		                   key[2], key[3], key[4], sample, sound.name);
	}
}

void AnimationSounds::Update()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::erase_if(g_Playing, [&registry](const Playing& playing) {
		if (!registry.Valid(playing.emitter) || !registry.AllOf<ecs::components::AudioEmitter>(playing.emitter))
		{
			return true;
		}
		if (registry.Valid(playing.owner))
		{
			if (const auto* at = registry.TryGet<const ecs::components::Transform>(playing.owner); at != nullptr)
			{
				registry.Get<ecs::components::Transform>(playing.emitter).position = at->position;
			}
		}
		return false;
	});
}

} // namespace openblack::audio
