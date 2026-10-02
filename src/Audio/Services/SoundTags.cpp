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

#include <algorithm>
#include <vector>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "Audio/Device/Sound.h"
#include "Audio/Services/Guidance.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{
/// SoundTag (0x54 bytes, SoundTag::Set 0x71E4F0)
struct Tag
{
	tags::TagId id {tags::k_NoTag};
	/// +0x0C: its thing; entt::null = none (a point tag, or a tag of a dead object)
	entt::entity thing {entt::null};
	/// (openblack) a tag of a ScriptMarker (agua's sound_tags): a thing that is always there, at `position`
	bool marker {false};
	/// +0x10: the point (the thing's when it was made, 0x71E336..0x71E3A1)
	glm::vec3 position {0.0f};
	/// +0x1C: added by the channel (options +0x3C)
	glm::vec3 offset {0.0f};
	int sample {0};                ///< +0x28
	/// +0x2C (the type). +0x34 = 0 from every ported Create, so GetBank 0x71E610 is GAudio+0x3A8 + 4 * type; only the
	/// atmos point tag fn_0071E920 (ctor fn_0071E460, the weather's thunder, not ported: Audio.h) sets it, for
	/// GAudio+0x194 + 4 * type
	SfxBank bank {SfxBank::None};
	bool track {false};            ///< +0x30: only with a thing (0x71E55D)
	int mode {3};                  ///< +0x38
	int loops {0};                 ///< +0x3C
	bool flag10 {false};           ///< +0x40 (the options' +0x10)
	bool is3D {false};             ///< +0x44
	int delay {0};                 ///< +0x48: only when is3D (0x71E56B)
	bool active {true};            ///< +0x4C (1 from Set, 0x71E4FA)
	uint16_t turns {0};            ///< +0x50: ProcessSoundTags calls since it was made
	bool gone {false};             ///< (openblack) deleted during a pass of the list

	[[nodiscard]] Owner ChannelOwner() const { return Owner::Tag(id); }
	[[nodiscard]] bool HasThing() const { return thing != entt::null || marker; }
};

/// g_game+0x205C1C in creation order (the original's head is the newest: ProcessSoundTags walks it from the back)
std::vector<Tag> g_Tags;
tags::TagId g_NextId = 1;

bool Trace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_SOUND_TAG_TRACE") != nullptr;
	return k_Trace;
}

Tag* Find(tags::TagId id)
{
	for (auto& tag : g_Tags)
	{
		if (tag.id == id && !tag.gone)
		{
			return &tag;
		}
	}
	return nullptr;
}

/// Get3DSoundPos of the tag's thing: a ScriptMarker's point, else the thing's (GameQueries); nullopt = gone
std::optional<glm::vec3> ThingPosition(const Tag& tag)
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

/// The tag deleted (its vtable +8): it leaves the list (the dtor fn_0071E5A0); its sample, if any, plays on
void Destroy(Tag& tag)
{
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sound tag {}: deleted (sample {})", tag.id, tag.sample);
	}
	tag.gone = true;
}

/// CreateSoundTagForDeadObject 0x71ECD0: 1 when the tag lives on (its loop released)
bool ForDeadObject(Tag& tag)
{
	tag.thing = entt::null; // 0x71ECD3
	tag.marker = false;
	const auto bank = Bank(tag.bank); // GetBank 0x71E610
	const auto sound = SampleId(bank, tag.sample);
	// fn_0042A460 (LHSampleGetInfo +0x40, the loops) and fn_0042A2D0 (LHSampleIsPlaying)
	if (sample_play::Loops(sound, tag.ChannelOwner()) != 0 && IsPlaying(tag.ChannelOwner(), tag.sample, bank))
	{
		// fn_0042A310: LHSampleReleaseLoop
		ReleaseLoop(tag.ChannelOwner(), tag.sample, bank);
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sound tag {}: thing gone, loop of {} released", tag.id, tag.sample);
		}
		return true;
	}
	return false;
}

/// SoundTag::ToBeDeleted 0x71ECB0
void ToBeDeleted(Tag& tag)
{
	if (!ForDeadObject(tag))
	{
		Destroy(tag);
	}
}

/// SoundTag::CheckDelay 0x71E760: the time since it was made, (+0x50) x [0xD01A38] ms (100 ms a turn, inferred:
/// villager_anims.md) x 0.001, at 347 a second ([0x980530], the speed of sound) against the camera's distance
void CheckDelay(Tag& tag)
{
	const auto camera = ListenerPoint();
	if (!camera)
	{
		return;
	}
	constexpr float k_MsPerTurn = 100.0f;    // [0xD01A38] (inferred)
	constexpr float k_SoundSpeed = 347.0f;   // [0x980530]
	const float seconds = static_cast<float>(tag.turns) * k_MsPerTurn * 0.001f; // 0x71E78D..0x71E79B
	const auto d = tag.position - *camera;   // GCamera::GetDistanceSq 0x71E7A6 of +0x10
	const float distanceSq = glm::dot(d, d);
	const auto bank = Bank(tag.bank);
	const float maxDistance = MaxDistance({bank, tag.sample}); // GetGSFXSampleMaxDistance 0x71E7C1
	const float reach = k_SoundSpeed * seconds;
	// 0x71E7D4: the sound has not reached the camera yet
	if (reach * reach < distanceSq)
	{
		return;
	}
	// 0x71E7E1..0x71E7F9: within the sample's max distance and active -> GAudio::PlaySoundEffect 0x42A040(tag, +0x10,
	// sample, mode, loops, +0x40, is3D, bank)
	if (distanceSq < maxDistance * maxDistance && tag.active)
	{
		PlaySoundEffectAt(tag.ChannelOwner(), tag.position, tag.sample, tag.mode, tag.loops, tag.flag10, tag.is3D, bank);
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sound tag {}: delayed {} heard after {} turns", tag.id, tag.sample,
			                   tag.turns);
		}
	}
	// 0x71E825: the delay is over, heard or not
	tag.delay = 0;
}

/// fn_0071E680, one tag of ProcessSoundTags
void Process(Tag& tag)
{
	++tag.turns; // 0x71E686
	if (tag.HasThing())
	{
		// 0x71E69A: GameThing::IsFunctional (vtable +0xD4) false -> ToBeDeleted; 0x71E6B8: Get3DSoundPos != 1 -> the
		// same (openblack: the thing's position is gone, inferred to be both)
		const auto at = ThingPosition(tag);
		if (!at)
		{
			ToBeDeleted(tag);
			return;
		}
		// 0x71E6A4: +0x4C
		if (!tag.active)
		{
			return;
		}
		// 0x71E6F2: GAudio::PlaySoundEffect 0x42A100(tag, pos, +0x1C, sample, +0x30, mode, loops, +0x40, is3D, bank)
		const auto channel = PlaySoundEffectAt(tag.ChannelOwner(), *at, tag.offset, tag.sample, tag.track, tag.mode,
		                                       tag.loops, tag.flag10, tag.is3D, Bank(tag.bank));
		if (Trace() && channel != k_NoChannel && tag.turns % 50 == 1)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sound tag {}: {} on channel {:#x} at ({:.1f}, {:.1f}, {:.1f})", tag.id,
			                   tag.sample, channel, at->x, at->y, at->z);
		}
		return;
	}
	// 0x71E71B: no thing: the delay, else gone once its sample stops (fn_0042A2D0)
	if (tag.delay != 0)
	{
		CheckDelay(tag);
		return;
	}
	if (!IsPlaying(tag.ChannelOwner(), tag.sample, Bank(tag.bank)))
	{
		Destroy(tag);
	}
}

void Compact()
{
	std::erase_if(g_Tags, [](const Tag& tag) { return tag.gone; });
}

tags::TagId Add(Tag tag)
{
	tag.id = g_NextId++;
	if (g_NextId == tags::k_NoTag)
	{
		g_NextId = 1;
	}
	// SoundTag::Set 0x71E4F0: the delay only for a 3D one, the track only with a thing; active 1, +0x50 = 0
	if (!tag.is3D)
	{
		tag.delay = 0;
	}
	if (!tag.HasThing())
	{
		tag.track = false;
	}
	g_Tags.push_back(tag);
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"),
		                   "Sound tag {}: made, sample {} bank {} mode {} loops {} {} {}, at ({:.1f}, {:.1f}, {:.1f})", tag.id,
		                   tag.sample, static_cast<int>(tag.bank), tag.mode, tag.loops, tag.is3D ? "3D" : "2D",
		                   tag.HasThing() ? "on a thing" : "on a point", tag.position.x, tag.position.y, tag.position.z);
	}
	return tag.id;
}
} // namespace

// ---- SoundTag::Create ---------------------------------------------------------------------------------------------

tags::TagId tags::Create(entt::entity thing, int sample, bool track, int mode, int loops, bool flag10, bool is3D,
                         SfxBank bank, int delay)
{
	// 0x71E840: the offset is (0, 0, 0) (0x71E84F..0x71E85F)
	return Create(thing, glm::vec3(0.0f), sample, track, mode, loops, flag10, is3D, bank, delay);
}

tags::TagId tags::Create(entt::entity thing, glm::vec3 offset, int sample, bool track, int mode, int loops, bool flag10,
                         bool is3D, SfxBank bank, int delay)
{
	// fn_0071E8C0 -> the ctor 0x71E300: the point is the thing's MapCoords (x, GetAltitude + y, z) (0x71E336..0x71E3A1)
	Tag tag;
	tag.thing = thing;
	tag.position = OwnerSoundPosition(Owner::Thing(thing)).value_or(glm::vec3(0.0f));
	tag.offset = offset;
	tag.sample = sample;
	tag.bank = bank;
	tag.track = track;
	tag.mode = mode;
	tag.loops = loops;
	tag.flag10 = flag10;
	tag.is3D = is3D;
	tag.delay = delay;
	return Add(tag);
}

tags::TagId tags::Create(glm::vec3 point, int sample, bool track, int mode, int loops, bool flag10, bool is3D,
                         SfxBank bank, int delay)
{
	// fn_0071EA40 -> fn_0071E3E0 (no thing, no offset)
	Tag tag;
	tag.position = point;
	tag.sample = sample;
	tag.bank = bank;
	tag.track = track;
	tag.mode = mode;
	tag.loops = loops;
	tag.flag10 = flag10;
	tag.is3D = is3D;
	tag.delay = delay;
	const auto id = Add(tag);
	// 0x71EABF..0x71EACC: played at once unless 3D with a delay
	if (is3D && delay != 0)
	{
		return id;
	}
	// 0x71EAD2..0x71EB33: new LH_SamplePlayOptions: bank +0x04, sample +0x24, the point +0x30, loops +0x4C, the tag
	// +0x20, is3D +0x08, track +0x0C = 0, mode +0x50 -> GAudio::PlaySoundEffect 0x429E30
	PlayOptions options;
	options.sample = {Bank(bank), sample};
	options.position = point;
	options.loops = loops;
	options.owner = Owner::Tag(id);
	options.is3D = is3D;
	options.track = false;
	options.mode = mode;
	PlaySoundEffect(options);
	return id;
}

tags::TagId tags::CreateAtMapCoords(float x, float z, float heightAboveLand, int sample, bool track, int mode, int loops,
                                    bool flag10, bool is3D, SfxBank bank, int delay)
{
	// 0x71EB60: LHPoint(x, GetAltitude(mc) + mc.y, z)
	return Create(glm::vec3(x, IslandAltitude(x, z) + heightAboveLand, z), sample, track, mode, loops, flag10, is3D, bank,
	              delay);
}

// ---- the others ---------------------------------------------------------------------------------------------------

void tags::SetActive(TagId id, bool active)
{
	auto* tag = Find(id);
	if (tag == nullptr)
	{
		return;
	}
	// fn_0071E640: an active tag turned off stops its sample (GAudio::StopPlayingSoundEffect 0x42A210)
	if (tag->active && !active)
	{
		StopSoundEffect(tag->sample, tag->ChannelOwner(), tag->bank);
	}
	tag->active = active;
}

void tags::Remove(entt::entity thing, int sample, SfxBank bank)
{
	// 0x71EBE0: every tag of fn_0071ED60 (thing +0xC, sample +0x28, type +0x2C) -> ToBeDeleted
	for (auto& tag : g_Tags)
	{
		if (!tag.gone && tag.thing == thing && tag.sample == sample && tag.bank == bank)
		{
			ToBeDeleted(tag);
		}
	}
	Compact();
}

void tags::Remove(entt::entity thing, int sample, SfxBank bank, bool stop)
{
	// 0x71EC30: the same, with GAudio::StopPlayingSoundEffect(sample, tag, type) first when stop (0x71EC60..0x71EC74)
	for (auto& tag : g_Tags)
	{
		if (!tag.gone && tag.thing == thing && tag.sample == sample && tag.bank == bank)
		{
			if (stop)
			{
				StopSoundEffect(tag.sample, tag.ChannelOwner(), tag.bank);
			}
			ToBeDeleted(tag);
		}
	}
	Compact();
}

void tags::Delete(TagId id)
{
	if (auto* tag = Find(id); tag != nullptr)
	{
		ToBeDeleted(*tag);
		Compact();
	}
}

int tags::RandomSample(int first, int count)
{
	// 0x71ED40: first + GRand::LocalRand(count) (0x6DE570: 0 for 0, else LHRand(count) on g_game+0x205A3C): the one
	// LocalRand of src/Audio, guidance::LocalRand (approximated there: openblack's generator, not LHRand's)
	if (count <= 0)
	{
		return first;
	}
	return first + static_cast<int>(guidance::LocalRand(static_cast<uint32_t>(count)));
}

bool tags::Exists(TagId id)
{
	return Find(id) != nullptr;
}

void tags::ProcessSoundTags()
{
	// 0x71E5F0: from the head (the newest); a tag may delete itself, the next one is read before (0x71E600)
	for (size_t i = g_Tags.size(); i-- > 0;)
	{
		if (!g_Tags[i].gone)
		{
			Process(g_Tags[i]);
		}
	}
	Compact();
}

void tags::Clear()
{
	for (auto& tag : g_Tags)
	{
		if (!tag.gone)
		{
			StopSoundEffect(tag.sample, tag.ChannelOwner(), tag.bank);
		}
	}
	g_Tags.clear();
}

std::optional<glm::vec3> tags::Get3DSoundPos(TagId id)
{
	const auto* tag = Find(id);
	if (tag == nullptr)
	{
		return std::nullopt;
	}
	// 0x71EC90: the thing's Get3DSoundPos; no thing: 1 without a point
	return ThingPosition(*tag);
}

std::optional<glm::vec3> tags::Point(TagId id)
{
	const auto* tag = Find(id);
	return tag != nullptr ? std::optional<glm::vec3>(tag->position) : std::nullopt;
}

// ---- agua's names -------------------------------------------------------------------------------------------------

sound_tags::TagId sound_tags::Create(const TagDesc& desc)
{
	// the waterfall's SoundTag::Create(marker, 12 G_WaterFlow, 0, 2, -1, 0, 1, InGame, 0) (0x5E3921); the sound id is
	// an InGame.sad one
	const auto* sound = sample_play::GetSound(desc.sample);
	const int sample = sound != nullptr ? sound->id : 0;
	const int loops = desc.loop ? -1 : 0;
	TagId id = k_NoTag;
	if (desc.thing != entt::null)
	{
		id = tags::Create(desc.thing, desc.point, sample, false, 2, loops, false, true, SfxBank::InGame, 0);
	}
	else
	{
		Tag tag;
		tag.marker = true;
		tag.position = desc.point;
		tag.sample = sample;
		tag.bank = SfxBank::InGame;
		tag.mode = 2;
		tag.loops = loops;
		tag.is3D = true;
		id = Add(tag);
	}
	if (!desc.active)
	{
		tags::SetActive(id, false);
	}
	return id;
}

void sound_tags::SetActive(TagId tag, bool active)
{
	tags::SetActive(tag, active);
}

void sound_tags::Delete(TagId tag)
{
	tags::Delete(tag);
}
