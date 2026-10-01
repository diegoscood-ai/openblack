/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SoundTags.h"

#include <cstdlib>
#include <vector>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "AudioManagerInterface.h"
#include "SamplePlay.h"
#include "Camera/Camera.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Sound.h"

using namespace openblack;
using namespace openblack::audio;
using namespace openblack::ecs::components;

namespace
{
/// A SoundTag. Its sample plays on a channel of audio::sample_play whose owner is the tag itself; `released` is a
/// deleted tag whose loop is finishing its pass (CreateSoundTagForDeadObject).
struct Tag
{
	sound_tags::TagId id {sound_tags::k_NoTag};
	sound_tags::TagDesc desc;
	bool released {false};

	[[nodiscard]] sample_play::Owner ChannelOwner() const { return sample_play::Owner::Tag(id); }
	[[nodiscard]] bool Playing() const { return sample_play::IsPlaying(desc.sample, ChannelOwner()); }
};

std::vector<Tag> g_Tags;
sound_tags::TagId g_NextId = 1;

bool Trace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_SOUND_TAG_TRACE") != nullptr;
	return k_Trace;
}

Tag* Find(sound_tags::TagId id)
{
	for (auto& tag : g_Tags)
	{
		if (tag.id == id && !tag.released)
		{
			return &tag;
		}
	}
	return nullptr;
}

/// GAudio::StopPlayingSoundEffect (LHSampleStop): the sample stops where it is
void HardStop(const Tag& tag)
{
	sample_play::Stop(tag.desc.sample, tag.ChannelOwner());
}
} // namespace

sound_tags::TagId sound_tags::Create(const TagDesc& desc)
{
	const TagId id = g_NextId++;
	g_Tags.push_back({id, desc, false});
	return id;
}

void sound_tags::SetActive(TagId id, bool active)
{
	auto* tag = Find(id);
	if (tag == nullptr)
	{
		return;
	}
	// fn_0071E640: only an active tag turned off stops its sample
	if (tag->desc.active && !active)
	{
		HardStop(*tag);
	}
	tag->desc.active = active;
}

void sound_tags::Delete(TagId id)
{
	auto* tag = Find(id);
	if (tag == nullptr)
	{
		return;
	}
	if (tag->desc.loop && tag->Playing())
	{
		// LHSampleReleaseLoop (0x42A310): it stops looping and ends with this pass; the tag lives until then
		sample_play::ReleaseLoop(tag->desc.sample, tag->ChannelOwner());
		tag->released = true;
		return;
	}
	// a one-shot tag that is still playing is deleted at once too, and its sample keeps playing to the end
	std::erase_if(g_Tags, [id](const Tag& other) { return other.id == id; });
}

void sound_tags::ProcessTurn()
{
	if (!Locator::audio::has_value() || !Locator::entitiesRegistry::has_value() || !Locator::camera::has_value() ||
	    !Locator::resources::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& sounds = Locator::resources::value().GetSounds();

	// the released tags that have stopped, and the tags whose thing is gone (GameThing::IsAvailable false ->
	// ToBeDeleted)
	std::vector<TagId> gone;
	std::erase_if(g_Tags, [&](Tag& tag) {
		if (tag.released)
		{
			return !tag.Playing();
		}
		if (tag.desc.thing != entt::null && !registry.Valid(tag.desc.thing))
		{
			gone.push_back(tag.id);
		}
		return false;
	});
	for (const auto id : gone)
	{
		Delete(id);
	}

	// the emitters are registry entities, so the points are read first
	std::vector<std::pair<size_t, glm::vec3>> toStart;
	for (size_t i = 0; i < g_Tags.size(); ++i)
	{
		const auto& tag = g_Tags[i];
		// SetActive(0) (+0x4C, 0x71E6A4): nothing
		if (tag.released || !tag.desc.active || !sounds.Contains(tag.desc.sample))
		{
			continue;
		}
		glm::vec3 at = tag.desc.point;
		if (tag.desc.thing != entt::null)
		{
			const auto* transform = registry.TryGet<const Transform>(tag.desc.thing);
			if (transform == nullptr)
			{
				continue;
			}
			// GameThingWithPos::Get3DSoundPos + the tag's offset
			at += transform->position;
		}
		toStart.emplace_back(i, at);
	}

	static uint32_t s_Turn = 0;
	if (Trace() && s_Turn++ % 50 == 0)
	{
		for (const auto& tag : g_Tags)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sound tag {}: sample {:#x}, active {}, playing {}, released {}", tag.id,
			                   tag.desc.sample, tag.desc.active, tag.Playing(), tag.released);
		}
	}

	for (const auto& [index, at] : toStart)
	{
		const auto& tag = g_Tags[index];
		const bool wasPlaying = tag.Playing();
		// fn_0071E680 -> GAudio::PlaySoundEffect 0x42A100 (owner = the tag, the point, the offset +0x1C, sample +0x28,
		// track +0x30 (only with a thing, and false in every ported Create), mode +0x38, loops +0x3C, is3D +0x44, the
		// tag's bank) -> PlaySoundEffect 0x429E30: not started beyond the sample's max distance from the camera; the
		// play mode 2 of the tags leaves a playing channel alone. The channel counts towards LHaudio's 16.
		sample_play::Options options;
		options.sound = tag.desc.sample;
		options.is3D = true;
		options.track = false;
		options.owner = tag.ChannelOwner();
		options.position = at;
		options.loops = tag.desc.loop ? -1 : 0;
		options.mode = 2;
		const auto emitter = sample_play::PlaySoundEffect(options);
		if (Trace() && emitter != entt::null && !wasPlaying)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sound tag {}: start of {:#x} at ({:.1f}, {:.1f}, {:.1f})", tag.id,
			                   tag.desc.sample, at.x, at.y, at.z);
		}
	}
}

void sound_tags::Clear()
{
	for (auto& tag : g_Tags)
	{
		HardStop(tag);
	}
	g_Tags.clear();
}
