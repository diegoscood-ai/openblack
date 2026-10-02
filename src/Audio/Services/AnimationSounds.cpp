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

#include <array>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "Audio/Audio.h"
#include "Audio/Device/Sound.h"
#include "Audio/GAudio/AudioSystem.h"
#include "Audio/GameQueries.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"

// A caller of the audio core (the animated things are ECS entities, read through GameQueries::animatedThing and
// animationClipName: this file includes no ECS component): the anim effects themselves, their tables and GAudio's
// filters are audio::SamplePlayAnimEffect (Audio.h, AnimEffects.h).

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

/// Data\SmallSounds.SAS as LoadAllAnimations 0x550180 keeps it in the clips (+0x44 group, +0x48 events)
struct Clips
{
	bool loaded {false};
	std::unordered_map<int32_t, ClipSounds> clips;
};

bool Trace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_ANIM_TRACE") != nullptr;
	return k_Trace;
}

Clips& Load()
{
	static Clips table;
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
				table.clips[index->second] = std::move(clip);
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
	// fn_00516510 0x516548: this->Get3DSoundPos(&p) (GameThingWithPos: its position), else nothing
	const auto thing = animatedThing(entity);
	const auto camera = ListenerPoint();
	if (!thing || !camera)
	{
		return;
	}
	const auto position = thing->position;
	// 0x51655F..0x5165B0: |p - LH3DTech::g_camera|, the distance of every event of the clip (also for banter heard at
	// the abode)
	const float distance = glm::distance(position, *camera);
	// 0x51662B: GSoundMap::GetSurfaceType(this->MapCoords) (GameQueries::surfaceType: agua's ecs::sea_cells, the single
	// source)
	const int32_t surface = SurfaceType(position);
	const auto& villager = thing->villager;
	const auto editor = Bank(SfxBank::Editor);                 // GAudio+0x3B0
	const auto banter = Bank(SfxBank::VillagersBanter);        // GAudio+0x3C8
	for (const auto& event : sounds->second.events)
	{
		if (event.time < from || event.time >= to)
		{
			continue;
		}
		int32_t voice = 2;
		if (sounds->second.group == 1)
		{
			// 0x5165BC: IsAlive (vtable +0x5B4), else the whole list is dropped
			if (villager && !villager->alive)
			{
				return;
			}
			voice = !villager || villager->child ? 3 : villager->female ? 2 : 1;
		}
		const AnimKey key = {voice, 2, sounds->second.group, surface, event.soundId};
		// fn_00516510's cases (0x51663A..0x5167A8)
		Owner owner = Owner::Thing(entity);
		BankId bank = editor;
		if (event.soundId >= 0x92 && event.soundId <= 0x94)
		{
			bank = banter;
			if (event.soundId == 0x92)
			{
				// 0x516751: IsVillager, else nothing; 0x51675D: Villager::GetAbode, passed as it is (no abode: owner 0, so
				// LHaudio puts it at the camera, fn_00427200 0x4272F9)
				if (!villager)
				{
					continue;
				}
				owner = villager->abode ? Owner::Thing(*villager->abode) : Owner::None();
			}
		}
		else
		{
			// 0x51665F..0x5166A5: a villager's footstep (4) is not heard while a script holds the wide screen (HelpSystem
			// +0x45E8 / +0x45EC) unless the villager is in a script (IsInScript, vtable +0x448: openblack has no
			// scripted villagers, inferred false). The jump goes to the function's end (0x5166A4 jne 0x5167B8): the rest of
			// the clip's events are dropped too, as for a dead villager.
			if (event.soundId == 4 && villager && IsScriptWideScreen())
			{
				return;
			}
			// 0x5166B1 / 0x5166F8: P_THROWN (399) and P_THROWN_VORTEX (401) only just after the throw
			// (TurnsSinceStateChange < 15 / < 10)
			const uint16_t turns = thing->turnsSinceStateChange;
			if ((clip == 399 && (!villager || turns >= 15)) || (clip == 401 && (!villager || turns >= 10)))
			{
				continue;
			}
		}
		if (Trace())
		{
			const auto* tables = anim_effects::Tables(bank);
			if (tables == nullptr || tables->FindList(key).empty())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Animation sound: clip {} id {} key {},{},{},{}: no row", clip,
				                   event.soundId, key[0], key[2], key[3], key[4]);
				continue;
			}
		}
		// 0x5167A8: GAudio::SamplePlayAnimEffect(owner, dist, key, action, bank, 1, 0.0f, 0.0f)
		const auto channel = SamplePlayAnimEffect(owner, distance, key, static_cast<AnimAction>(event.action), bank, true,
		                                          0.0f, 0.0f);
		if (Trace())
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
	// Tree::Draw: SamplePlayAnimEffect(tree, |tree - camera| (0x74AFFD / 0x74B250), key, 0, editor (GAudio+0x3B0),
	// track, 0, 0). The two callers of PlayFromTable are those two sites: the bend ({c, *, *, 10, 75}, 0x74B009) passes
	// track 0 (push ebp = 0, 0x74AFE1), the ambient rustle ({*, *, 20, *, 70}, 0x74B25C) track 1 (push 1, 0x74B1FA).
	// PlayFromTable has no track argument (its signature stays for ECS/Trees.cpp), so the site is told by the key's
	// soundId (openblack)
	const bool track = key[4] == 70;
	const auto bank = Bank(SfxBank::Editor);
	const auto channel = SamplePlayAnimEffect(Owner::Thing(owner), glm::distance(position, *camera), key, AnimAction::Play,
	                                          bank, track, 0.0f, 0.0f);
	if (Trace() && channel != k_NoChannel)
	{
		if (const auto* sound = sample_play::GetSound(sample_play::SoundOf(channel)); sound != nullptr)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Animation sound: key {},{},{},{},{} -> editor.sad/{} ({})", key[0],
			                   key[1], key[2], key[3], key[4], sound->id, sound->name);
		}
	}
}

void AnimationSounds::Update()
{
	// The channels follow their owner once a turn (LHSampleUpdate3DChannels, audio::ProcessTurn): nothing per frame.
}

} // namespace openblack::audio
