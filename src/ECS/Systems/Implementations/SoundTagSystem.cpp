/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "SoundTagSystem.h"

#include <cstdlib>

#include <algorithm>
#include <string>
#include <type_traits>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "Audio/Audio.h"
#include "Audio/Engine/SamplePlay.h"
#include "Audio/Services/SoundMap.h"
#include "GameClock.h"

using namespace openblack;
using namespace openblack::audio;
using namespace openblack::ecs::systems;
using SoundTag = openblack::ecs::systems::SoundTagSystem::Tag;

static_assert(std::is_same_v<SoundTagSystemInterface::TagId, tags::TagId>);

namespace
{
bool SoundTagTrace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_SOUND_TAG_TRACE") != nullptr;
	return k_Trace;
}

/// The tag is the owner of its channel
Owner TagChannelOwner(const SoundTag& tag)
{
	return Owner::Tag(tag.id);
}

bool TagHasThing(const SoundTag& tag)
{
	return tag.thing != entt::null || tag.marker;
}

/// The ambient bank of an ambient type (k_NoBank for none), found by its file name as the atmos mixer registers it
BankId AmbientBankOf(AtmosType type)
{
	const auto index = static_cast<size_t>(type);
	if (index >= k_AtmosTypeCount || k_AtmosTypes[index].bank == nullptr)
	{
		return k_NoBank;
	}
	return FindBank(std::string("/") + k_AtmosTypes[index].bank);
}

/// The bank the tag's sample is in: an ambient bank for the ambient point tag, else its sound effect bank
BankId TagBankOf(const SoundTag& tag)
{
	if (tag.ambientBank != AtmosType::None)
	{
		return AmbientBankOf(tag.ambientBank);
	}
	return Bank(tag.bank);
}

void StopTagSample(const SoundTag& tag)
{
	if (tag.ambientBank != AtmosType::None)
	{
		StopSoundEffect(tag.sample, TagChannelOwner(tag), TagBankOf(tag));
		return;
	}
	StopSoundEffect(tag.sample, TagChannelOwner(tag), tag.bank);
}

/// The sound position of the tag's thing: a script marker's point, else the thing's; nullopt when it has gone
std::optional<glm::vec3> TagThingPosition(const SoundTag& tag)
{
	if (tag.marker)
	{
		return tag.position;
	}
	if (tag.thing == entt::null)
	{
		return std::nullopt;
	}
	return OwnerSoundPosition(Owner::Thing(tag.thing));
}

/// The tag deleted: it leaves the list; its sample, if any, plays on
void DestroyTag(SoundTag& tag)
{
	if (SoundTagTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sound tag {}: deleted (sample {})", tag.id, tag.sample);
	}
	tag.gone = true;
}

/// The tag's thing has gone: true when the tag lives on (its loop released)
bool TagForDeadObject(SoundTag& tag)
{
	tag.thing = entt::null;
	tag.marker = false;
	const auto bank = TagBankOf(tag);
	const auto sound = SampleId(bank, tag.sample);
	// a looping sample that still plays
	if (sample_play::Loops(sound, TagChannelOwner(tag)) != 0 && IsPlaying(TagChannelOwner(tag), tag.sample, bank))
	{
		ReleaseLoop(TagChannelOwner(tag), tag.sample, bank);
		if (SoundTagTrace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sound tag {}: thing gone, loop of {} released", tag.id, tag.sample);
		}
		return true;
	}
	return false;
}

void TagToBeDeleted(SoundTag& tag)
{
	if (!TagForDeadObject(tag))
	{
		DestroyTag(tag);
	}
}

/// The time since it was made (turns of game_clock::MsPerTurn() ms, 100 by default) at 347 m a second, the speed of
/// sound, against the camera's distance
void CheckTagDelay(SoundTag& tag)
{
	const auto camera = ListenerPoint();
	if (!camera)
	{
		return;
	}
	constexpr float k_SoundSpeed = 347.0f; // metres a second
	// turns x ms per turn, in seconds
	const float seconds = static_cast<float>(tag.turns) * static_cast<float>(game_clock::MsPerTurn()) * 0.001f;
	const auto d = tag.position - *camera;
	const float distanceSq = glm::dot(d, d);
	const auto bank = TagBankOf(tag);
	const float maxDistance = MaxDistance({bank, tag.sample});
	const float reach = k_SoundSpeed * seconds;
	// the sound has not reached the camera yet
	if (reach * reach < distanceSq)
	{
		return;
	}
	// within the sample's max distance and active: played at the tag's point
	if (distanceSq < maxDistance * maxDistance && tag.active)
	{
		PlaySoundEffectAt(TagChannelOwner(tag), tag.position, tag.sample, tag.mode, tag.loops, tag.extra3DFlag, tag.is3D, bank);
		if (SoundTagTrace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sound tag {}: delayed {} heard after {} turns", tag.id, tag.sample,
			                   tag.turns);
		}
	}
	// the delay is over, heard or not
	tag.delay = 0;
}

/// One tag of the turn
void ProcessTag(SoundTag& tag)
{
	++tag.turns;
	if (TagHasThing(tag))
	{
		// a thing that is no longer functional, or has no sound position, deletes the tag (here both show as the
		// thing's position being gone)
		const auto at = TagThingPosition(tag);
		if (!at)
		{
			TagToBeDeleted(tag);
			return;
		}
		if (!tag.active)
		{
			return;
		}
		// replayed at the thing every turn
		const auto channel = PlaySoundEffectAt(TagChannelOwner(tag), *at, tag.offset, tag.sample, tag.track, tag.mode,
		                                       tag.loops, tag.extra3DFlag, tag.is3D, TagBankOf(tag));
		if (SoundTagTrace() && channel != k_NoChannel && tag.turns % 50 == 1)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sound tag {}: {} on channel {:#x} at ({:.1f}, {:.1f}, {:.1f})", tag.id,
			                   tag.sample, channel, at->x, at->y, at->z);
		}
		return;
	}
	// no thing: the delay, else gone once its sample stops
	if (tag.delay != 0)
	{
		CheckTagDelay(tag);
		return;
	}
	if (!IsPlaying(TagChannelOwner(tag), tag.sample, TagBankOf(tag)))
	{
		DestroyTag(tag);
	}
}
} // namespace

SoundTag* SoundTagSystem::Find(TagId tag)
{
	const auto found = std::ranges::find_if(_tags, [tag](const SoundTag& entry) { return entry.id == tag && !entry.gone; });
	return found != _tags.end() ? &*found : nullptr;
}

const SoundTag* SoundTagSystem::Find(TagId tag) const
{
	const auto found = std::ranges::find_if(_tags, [tag](const SoundTag& entry) { return entry.id == tag && !entry.gone; });
	return found != _tags.end() ? &*found : nullptr;
}

SoundTagSystem::TagId SoundTagSystem::Add(SoundTag tag)
{
	tag.id = _nextId++;
	if (_nextId == tags::k_NoTag)
	{
		_nextId = 1;
	}
	// the delay only for a 3D one, the track only with a thing; active, no turns yet
	if (!tag.is3D)
	{
		tag.delay = 0;
	}
	if (!TagHasThing(tag))
	{
		tag.track = false;
	}
	_tags.push_back(tag);
	if (SoundTagTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"),
		                   "Sound tag {}: made, sample {} bank {} mode {} loops {} {} {}, at ({:.1f}, {:.1f}, {:.1f})", tag.id,
		                   tag.sample, static_cast<int>(tag.bank), tag.mode, tag.loops, tag.is3D ? "3D" : "2D",
		                   TagHasThing(tag) ? "on a thing" : "on a point", tag.position.x, tag.position.y, tag.position.z);
	}
	return tag.id;
}

void SoundTagSystem::Compact()
{
	std::erase_if(_tags, [](const SoundTag& tag) { return tag.gone; });
}

void SoundTagSystem::ProcessTurn()
{
	// from the head (the newest); a tag may delete itself, so the walk is by index and the gone ones go after it
	for (size_t i = _tags.size(); i-- > 0;)
	{
		if (!_tags[i].gone)
		{
			ProcessTag(_tags[i]);
		}
	}
	Compact();
}

void SoundTagSystem::SetActive(TagId tag, bool active)
{
	auto* entry = Find(tag);
	if (entry == nullptr)
	{
		return;
	}
	// an active tag turned off stops its sample
	if (entry->active && !active)
	{
		StopTagSample(*entry);
	}
	entry->active = active;
}

SoundTagSystem::TagId SoundTagSystem::AddPointSound(SoundTag tag)
{
	const auto id = Add(tag);
	// played at once unless 3D with a delay
	if (tag.is3D && tag.delay != 0)
	{
		return id;
	}
	// at the point, owned by the tag, without tracking
	PlayOptions options;
	options.sample = {TagBankOf(tag), tag.sample};
	options.position = tag.position;
	options.loops = tag.loops;
	options.owner = Owner::Tag(id);
	options.is3D = tag.is3D;
	options.track = false;
	options.mode = tag.mode;
	PlaySoundEffect(options);
	return id;
}

SoundTagSystem::TagId SoundTagSystem::CreatePointSound(const glm::vec3& point, int sample, int mode, int loops,
                                                       bool extra3DFlag, bool is3D, SfxBank bank, int delay)
{
	// no thing, no offset
	return AddPointSound(SoundTag {
	    .position = point,
	    .sample = sample,
	    .bank = bank,
	    .mode = mode,
	    .loops = loops,
	    .extra3DFlag = extra3DFlag,
	    .is3D = is3D,
	    .delay = delay,
	});
}

SoundTagSystem::TagId SoundTagSystem::CreatePointSound(const glm::vec3& point, int sample, int mode, int loops,
                                                       bool extra3DFlag, bool is3D, AtmosType bank, int delay)
{
	// the same with the sample in an ambient bank
	return AddPointSound(SoundTag {
	    .position = point,
	    .sample = sample,
	    .ambientBank = bank,
	    .mode = mode,
	    .loops = loops,
	    .extra3DFlag = extra3DFlag,
	    .is3D = is3D,
	    .delay = delay,
	});
}

SoundTagSystem::TagId SoundTagSystem::Create(entt::entity thing, const glm::vec3& offset, int sample, bool track, int mode,
                                             int loops, bool extra3DFlag, bool is3D, SfxBank bank, int delay)
{
	// the point is the thing's (x, ground + y, z)
	return Add(SoundTag {
	    .thing = thing,
	    .position = OwnerSoundPosition(Owner::Thing(thing)).value_or(glm::vec3(0.0f)),
	    .offset = offset,
	    .sample = sample,
	    .bank = bank,
	    .track = track,
	    .mode = mode,
	    .loops = loops,
	    .extra3DFlag = extra3DFlag,
	    .is3D = is3D,
	    .delay = delay,
	});
}

SoundTagSystem::TagId SoundTagSystem::CreateAtMarker(const glm::vec3& point, int sample, int mode, int loops, bool is3D,
                                                     SfxBank bank)
{
	return Add(SoundTag {
	    .marker = true,
	    .position = point,
	    .sample = sample,
	    .bank = bank,
	    .mode = mode,
	    .loops = loops,
	    .is3D = is3D,
	});
}

void SoundTagSystem::Remove(entt::entity thing, int sample, SfxBank bank, bool stop)
{
	// every tag of that thing, sample and bank is deleted, stopping its sample first when stop
	for (auto& tag : _tags)
	{
		if (!tag.gone && tag.thing == thing && tag.sample == sample && tag.bank == bank)
		{
			if (stop)
			{
				StopTagSample(tag);
			}
			TagToBeDeleted(tag);
		}
	}
	Compact();
}

void SoundTagSystem::Delete(TagId tag)
{
	if (auto* entry = Find(tag); entry != nullptr)
	{
		TagToBeDeleted(*entry);
		Compact();
	}
}

bool SoundTagSystem::Exists(TagId tag) const
{
	return Find(tag) != nullptr;
}

void SoundTagSystem::Clear()
{
	for (const auto& tag : _tags)
	{
		if (!tag.gone)
		{
			StopTagSample(tag);
		}
	}
	_tags.clear();
}

std::optional<glm::vec3> SoundTagSystem::TagSoundPoint(TagId tag) const
{
	const auto* entry = Find(tag);
	if (entry == nullptr)
	{
		return std::nullopt;
	}
	// the thing's sound position; no thing: none
	return TagThingPosition(*entry);
}

std::optional<glm::vec3> SoundTagSystem::Point(TagId tag) const
{
	const auto* entry = Find(tag);
	return entry != nullptr ? std::optional<glm::vec3>(entry->position) : std::nullopt;
}
