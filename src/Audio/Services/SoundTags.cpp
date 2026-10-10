/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The functions of audio::tags: each one hands its call to the sound tag system (Locator::soundTagSystem)

#include "SoundTags.h"

#include <stdexcept>

#include "3D/Lightning.h"
#include "3D/MapCoords.h"
#include "Audio/Device/Sound.h"
#include "Audio/Services/Guidance.h"
#include "Audio/Services/SoundMap.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{
ecs::systems::SoundTagSystemInterface& SoundTagService()
{
	if (!openblack::Locator::soundTagSystem::has_value())
	{
		throw std::logic_error("audio: no sound tag system (Locator::soundTagSystem is empty)");
	}
	return openblack::Locator::soundTagSystem::value();
}
} // namespace

// ---- Create -------------------------------------------------------------------------------------------------------

tags::TagId tags::Create(entt::entity thing, int sample, bool track, int mode, int loops, bool extra3DFlag, bool is3D,
                         SfxBank bank, int delay)
{
	// the offset is (0, 0, 0)
	return Create(thing, glm::vec3(0.0f), sample, track, mode, loops, extra3DFlag, is3D, bank, delay);
}

tags::TagId tags::Create(entt::entity thing, glm::vec3 offset, int sample, bool track, int mode, int loops, bool extra3DFlag,
                         bool is3D, SfxBank bank, int delay)
{
	return SoundTagService().Create(thing, offset, sample, track, mode, loops, extra3DFlag, is3D, bank, delay);
}

tags::TagId tags::Create(glm::vec3 point, int sample, bool /*track*/, int mode, int loops, bool extra3DFlag, bool is3D,
                         SfxBank bank, int delay)
{
	// a point's tag has no thing to track
	return SoundTagService().CreatePointSound(point, sample, mode, loops, extra3DFlag, is3D, bank, delay);
}

tags::TagId tags::CreateAtMapCoords(const map_coords::MapCoords& coords, int sample, bool track, int mode, int loops,
                                    bool extra3DFlag, bool is3D, SfxBank bank, int delay)
{
	// (x, ground + altitude, z), x and z in metres (map_coords::ToMetres)
	return CreateAtMapCoords(map_coords::ToMetres(coords.x), map_coords::ToMetres(coords.z), coords.altitude, sample, track,
	                         mode, loops, extra3DFlag, is3D, bank, delay);
}

tags::TagId tags::CreateAtMapCoords(float x, float z, float heightAboveLand, int sample, bool track, int mode, int loops,
                                    bool extra3DFlag, bool is3D, SfxBank bank, int delay)
{
	// On map coordinates already in metres (x, z = ToMetres of its 16.16 values: magic::ToMap): not quantised again
	// (a second round trip may lose a unit). The point is (x, ground + altitude, z)
	return Create(glm::vec3(x, IslandAltitude(x, z) + heightAboveLand, z), sample, track, mode, loops, extra3DFlag, is3D, bank,
	              delay);
}

tags::TagId tags::Thunder(ecs::systems::SoundTagSystemInterface& soundTags, glm::vec3 point, uint32_t ticks)
{
	// The thunder's tag: mode 2, no loops, 3D, in the rain's ambient bank, delayed until the sound reaches the camera
	constexpr int k_Mode = 2;
	constexpr int k_Loops = 0;
	constexpr int k_Delay = 1;
	return soundTags.CreatePointSound(point, lightning::ThunderClap(ticks), k_Mode, k_Loops, false, true, AtmosType::Rain,
	                                  k_Delay);
}

tags::TagId tags::Thunder(glm::vec3 point)
{
	return Thunder(SoundTagService(), point, audio::TickCount());
}

// ---- the others ---------------------------------------------------------------------------------------------------

void tags::SetActive(TagId id, bool active)
{
	SoundTagService().SetActive(id, active);
}

void tags::Remove(entt::entity thing, int sample, SfxBank bank)
{
	// every tag of the three is deleted, its sample left to play on
	SoundTagService().Remove(thing, sample, bank, false);
}

void tags::Remove(entt::entity thing, int sample, SfxBank bank, bool stop)
{
	SoundTagService().Remove(thing, sample, bank, stop);
}

void tags::Delete(TagId id)
{
	SoundTagService().Delete(id);
}

int tags::RandomSample(int first, int count)
{
	// first + a local random number below count (0 for 0): the one local random of src/Audio, guidance::LocalRand
	// (game_random's local stream)
	if (count <= 0)
	{
		return first;
	}
	return first + static_cast<int>(guidance::LocalRand(static_cast<uint32_t>(count)));
}

bool tags::Exists(TagId id)
{
	return SoundTagService().Exists(id);
}

void tags::ProcessSoundTags()
{
	SoundTagService().ProcessTurn();
}

void tags::Clear()
{
	SoundTagService().Clear();
}

std::optional<glm::vec3> tags::TagSoundPoint(TagId id)
{
	return SoundTagService().TagSoundPoint(id);
}

std::optional<glm::vec3> tags::Point(TagId id)
{
	return SoundTagService().Point(id);
}

// ---- the sound_tags names -----------------------------------------------------------------------------------------

sound_tags::TagId sound_tags::Create(const TagDesc& desc)
{
	// as the waterfall's tag (marker, sample 12 G_WaterFlow, mode 2, for ever, 3D, InGame); the sound id is
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
		id = SoundTagService().CreateAtMarker(desc.point, sample, 2, loops, true, SfxBank::InGame);
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
