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

#include "3D/L3DAnim.h"
#include "Audio/Audio.h"
#include "Audio/Sound.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "ECS/Animations.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

// A caller of the audio core (the animated things are ECS entities): the anim effects themselves, their tables and
// GAudio's filters are audio::SamplePlayAnimEffect (Audio.h, AnimEffects.h).

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
	if (!Locator::entitiesRegistry::has_value() || from >= to)
	{
		return;
	}
	const auto& clips = Load();
	const auto sounds = clips.clips.find(clip);
	if (sounds == clips.clips.end())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// fn_00516510 0x516548: this->Get3DSoundPos(&p) (GameThingWithPos: its position), else nothing
	const auto* transform = registry.TryGet<const ecs::components::Transform>(entity);
	const auto camera = ListenerPoint();
	if (transform == nullptr || !camera)
	{
		return;
	}
	const auto position = transform->position;
	// 0x51655F..0x5165B0: |p - LH3DTech::g_camera|, the distance of every event of the clip (also for banter heard at
	// the abode)
	const float distance = glm::distance(position, *camera);
	// 0x51662B: GSoundMap::GetSurfaceType(this->MapCoords) (agua's ecs::sea_cells, the single source)
	const int32_t surface = ecs::sea_cells::GetSurfaceType(position);
	const auto* villager = registry.TryGet<const ecs::components::Villager>(entity);
	const auto* action = registry.TryGet<const ecs::components::LivingAction>(entity);
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
			if (villager != nullptr && villager->life <= 0.0f)
			{
				return;
			}
			voice = villager == nullptr || villager->lifeStage == ecs::components::Villager::LifeStage::Child ? 3
			        : villager->sex == ecs::components::Villager::Sex::FEMALE                              ? 2
			                                                                                                : 1;
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
				if (villager == nullptr)
				{
					continue;
				}
				owner = villager->abode != entt::null && registry.Valid(villager->abode) ? Owner::Thing(villager->abode)
				                                                                        : Owner::None();
			}
		}
		else
		{
			// 0x51665F..0x5166A5: a villager's footstep (4) is not heard while a script holds the wide screen (HelpSystem
			// +0x45E8 / +0x45EC) unless the villager is in a script (IsInScript, vtable +0x448: openblack has no
			// scripted villagers, inferred false). The jump goes to the function's end (0x5166A4 jne 0x5167B8): the rest of
			// the clip's events are dropped too, as for a dead villager.
			if (event.soundId == 4 && villager != nullptr && IsScriptWideScreen())
			{
				return;
			}
			// 0x5166B1 / 0x5166F8: P_THROWN (399) and P_THROWN_VORTEX (401) only just after the throw
			// (TurnsSinceStateChange < 15 / < 10)
			const uint16_t turns = action != nullptr ? action->turnsSinceStateChange : 0;
			if ((clip == 399 && (villager == nullptr || turns >= 15)) || (clip == 401 && (villager == nullptr || turns >= 10)))
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

void AnimationSounds::RunTestHooks(uint32_t turn)
{
	const char* view = std::getenv("OPENBLACK_AUDIO_TEST_VIEW");
	if (view == nullptr || !Locator::entitiesRegistry::has_value() || !Locator::camera::has_value())
	{
		return;
	}
	unsigned at = 0;
	int wanted = 0;
	float distance = 4.0f;
	if (std::sscanf(view, "%u,%d,%f", &at, &wanted, &distance) < 2 || turn != at)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	int clip = -1;
	if (const char* anim = std::getenv("OPENBLACK_AUDIO_TEST_ANIM"); anim != nullptr)
	{
		clip = std::atoi(anim);
	}
	int index = 0;
	registry.Each<const ecs::components::Villager, const ecs::components::Transform>(
	    [&](entt::entity entity, const ecs::components::Villager&, const ecs::components::Transform& t) {
		    if (clip >= 0)
		    {
			    auto& animation = registry.AssignOrReplace<ecs::components::SkeletalAnimation>(entity);
			    animation.clip = ecs::ClipId(static_cast<uint32_t>(clip));
			    animation.clipIndex = clip;
			    animation.locked = true;
			    animation.hasClip = true;
			    animation.time = 0.0f;
			    animation.speed = 1.0f;
		    }
		    if (index++ != wanted)
		    {
			    return;
		    }
		    const glm::vec3 focus = t.position + glm::vec3(0.0f, 0.8f, 0.0f);
		    Locator::camera::value().GetModel().SetFlight(focus + glm::vec3(0.0f, distance * 0.35f, distance), focus);
		    SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Audio test: turn {}, camera on villager {} at ({:.1f}, {:.1f}, {:.1f}), clip {}",
		                       turn, wanted, t.position.x, t.position.y, t.position.z, clip);
	    });
}

} // namespace openblack::audio
