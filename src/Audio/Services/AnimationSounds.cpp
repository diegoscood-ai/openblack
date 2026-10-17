/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AnimationSounds.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "Audio/Audio.h"
#include "Audio/ClipSounds.h"
#include "Audio/Device/Sound.h"
#include "Audio/Game/AudioSystem.h"
#include "Audio/GameQueries.h"
#include "Debug/DebugEnv.h"
#include "Debug/TraceLog.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

// A caller of the audio core (the animated things are ECS entities, read through GameQueries::animatedThing and
// animationClipName: this file includes no ECS component): the anim effects themselves, their tables and the play
// filters are audio::PlayAnimationEffect (Audio.h, AnimEffects.h).

namespace openblack::audio
{
namespace
{
constexpr int32_t k_ClipCount = 441;

/// Data\SmallSounds.SAS as the animation loading keeps it in the clips (a group and the events)
struct AnimationClips
{
	bool loaded {false};
	std::unordered_map<int32_t, clip_sounds::Clip> clips;
};

bool Trace()
{
	static const bool k_Trace = debug_env::AnimTrace();
	return k_Trace;
}

AnimationClips& Load()
{
	// the parsed table, kept by the audio state (Locator::audioState)
	auto& table = openblack::Locator::audioState::value().Get<AnimationClips>();
	if (table.loaded)
	{
		return table;
	}
	table.loaded = true;
	auto& fileSystem = Locator::filesystem::value();
	std::unordered_map<std::string, int32_t> byName;
	if (const auto& clipName = Queries().animationClipName; clipName)
	{
		for (int32_t i = 0; i < k_ClipCount; ++i)
		{
			if (const auto name = clipName(i); name)
			{
				byName.emplace(*name, i);
			}
		}
	}
	try
	{
		// the file read once into the resources; each name's sounds go to its clip, a later one of a name over an earlier
		auto& tables = Locator::resources::value().GetClipSounds();
		if (!tables.Contains(clip_sounds::k_TableId.value()))
		{
			const auto& bytes = resources::LoadBlob(Locator::resources::value().GetBlobs(),
			                                        fileSystem.GetPath<filesystem::Path::Data>() / "SmallSounds.SAS");
			tables.Load(clip_sounds::k_TableId.value(), resources::ClipSoundsLoader::FromBufferTag {}, bytes);
		}
		for (const auto& clip : tables.Handle(clip_sounds::k_TableId.value())->clips)
		{
			if (const auto index = byName.find(clip.name); index != byName.end())
			{
				table.clips[index->second] = clip;
			}
		}
	}
	catch (const std::exception& error)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("audio"), "Animation sounds: {}", error.what());
	}
	const auto* editor = anim_effects::Tables(Bank(SfxBank::Editor));
	const auto* banter = anim_effects::Tables(Bank(SfxBank::VillagersBanter));
	SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Animation sounds: {} clips, {} + {} effect rows", table.clips.size(),
	                   editor != nullptr ? editor->rows.size() : 0, banter != nullptr ? banter->rows.size() : 0);
	return table;
}

/// The sample a play put on its channel, for the trace
void TracePlay(Channel channel, int32_t clip, int32_t time, int32_t soundId, const AnimKey& key, BankId bank)
{
	if (channel == k_NoChannel)
	{
		return;
	}
	const auto* sound = sample_play::GetSound(sample_play::SoundOf(channel));
	if (sound == nullptr)
	{
		return;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Animation sound: clip {} at {} ms, id {} surface {} -> {}/{} ({})", clip, time,
	                   soundId, key[3], BankGroup(bank), sound->id, sound->name);
}
} // namespace

void AnimationSounds::Fire(entt::entity entity, int32_t clip, int32_t from, int32_t to)
{
	const auto& animatedThing = Queries().animatedThing;
	if (!animatedThing || from >= to)
	{
		return;
	}
	const auto& clips = Load();
	const auto sounds = clips.clips.find(clip);
	if (sounds == clips.clips.end())
	{
		return;
	}
	// no event of the clip in [from, to): nothing would fire, so the thing, camera and surface are not looked up
	if (std::ranges::none_of(sounds->second.events,
	                         [from, to](const auto& event) { return event.time >= from && event.time < to; }))
	{
		return;
	}
	// the thing's sound position (its position), else nothing
	const auto thing = animatedThing(entity);
	const auto camera = ListenerPoint();
	if (!thing || !camera)
	{
		return;
	}
	const auto position = thing->position;
	// the camera's distance to the thing, for every event of the clip (also for banter heard at the abode)
	const float distance = glm::distance(position, *camera);
	// the surface under the thing (GameQueries::surfaceType: ecs::sea_cells, the single source)
	const int32_t surface = SurfaceType(position);
	const auto& villager = thing->villager;
	const EventThing speaker {
	    .alive = thing->alive,
	    .villager = villager.has_value(),
	    .child = villager && villager->child,
	    .female = villager && villager->female,
	};
	const auto editor = Bank(SfxBank::Editor);
	const auto banter = Bank(SfxBank::VillagersBanter);
	for (const auto& event : sounds->second.events)
	{
		if (event.time < from || event.time >= to)
		{
			continue;
		}
		// the villagers' voices group speaks only from a living thing, else the rest of the clip's events are dropped
		const auto voice = EventVoice(sounds->second.group, speaker);
		if (!voice)
		{
			return;
		}
		const AnimKey key = {*voice, 2, sounds->second.group, surface, event.soundId};
		// the owner and bank by sound id
		Owner owner = Owner::Thing(entity);
		BankId bank = editor;
		if (event.soundId >= 0x92 && event.soundId <= 0x94)
		{
			bank = banter;
			if (event.soundId == 0x92)
			{
				// only a villager; its abode, passed as it is (no abode: no owner, so the sample plays at the
				// camera)
				if (!villager)
				{
					continue;
				}
				owner = villager->abode ? Owner::Thing(*villager->abode) : Owner::None();
			}
		}
		else
		{
			// a villager's footstep (4) is not heard while a script holds the wide screen unless the villager is in a
			// script (openblack has no scripted villagers, inferred false). The rest of the clip's events are dropped
			// too, as for a dead villager.
			if (event.soundId == 4 && villager && IsScriptWideScreen())
			{
				return;
			}
			// P_THROWN (399) and P_THROWN_VORTEX (401) only just after the throw
			// (TurnsSinceStateChange < 15 / < 10)
			const uint16_t turns = thing->turnsSinceStateChange;
			if ((clip == 399 && (!villager || turns >= 15)) || (clip == 401 && (!villager || turns >= 10)))
			{
				continue;
			}
		}
		// A key with no row still goes to PlayAnimationEffect, as it does untraced: the trace only says so
		if (debug_env::TraceLogs(Trace(), "audio"))
		{
			const auto* tables = anim_effects::Tables(bank);
			if (tables == nullptr || tables->FindList(key).empty())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Animation sound: clip {} id {} key {},{},{},{}: no row", clip,
				                   event.soundId, key[0], key[2], key[3], key[4]);
			}
		}
		// tracking its owner, min and max distance 0
		const auto channel =
		    PlayAnimationEffect(owner, distance, key, static_cast<AnimAction>(event.action), bank, true, 0.0f, 0.0f);
		if (debug_env::TraceLogs(Trace(), "audio"))
		{
			TracePlay(channel, clip, event.time, event.soundId, key, bank);
		}
	}
}

void AnimationSounds::PlayFromTable(entt::entity owner, glm::vec3 position, const std::array<int32_t, 5>& key)
{
	const auto camera = ListenerPoint();
	if (!camera)
	{
		return;
	}
	// The tree drawing plays the editor bank at the camera's distance to the tree. The two callers of PlayFromTable are
	// its two sites: the bend ({c, *, *, 10, 75}) without tracking, the ambient rustle ({*, *, 20, *, 70}) with it.
	// PlayFromTable has no track argument (its signature stays for ECS/Trees.cpp), so the site is told by the key's
	// soundId (openblack)
	const bool track = key[4] == 70;
	const auto bank = Bank(SfxBank::Editor);
	const auto channel = PlayAnimationEffect(Owner::Thing(owner), glm::distance(position, *camera), key, AnimAction::Play, bank,
	                                         track, 0.0f, 0.0f);
	if (debug_env::TraceLogs(Trace(), "audio") && channel != k_NoChannel)
	{
		if (const auto* sound = sample_play::GetSound(sample_play::SoundOf(channel)); sound != nullptr)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Animation sound: key {},{},{},{},{} -> editor.sad/{} ({})", key[0],
			                   key[1], key[2], key[3], key[4], sound->id, sound->name);
		}
	}
}

std::optional<int32_t> AnimationSounds::EventVoice(int32_t group, const EventThing& thing)
{
	if (group != 1)
	{
		return 2;
	}
	if (!thing.alive)
	{
		return std::nullopt;
	}
	if (!thing.villager || thing.child)
	{
		return 3;
	}
	return thing.female ? 2 : 1;
}

void AnimationSounds::Update()
{
	// The channels follow their owner once a turn (audio::ProcessTurn): nothing per frame.
}

} // namespace openblack::audio
