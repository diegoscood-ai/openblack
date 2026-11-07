/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack::audio
{
enum class SfxBank : uint8_t;
enum class AtmosType : uint8_t;
} // namespace openblack::audio

namespace openblack::ecs::systems
{

/// The sounds things keep up and the sounds made at points (sound tags). The functions of audio::tags
/// (Audio.h) hand their calls to it.
///
/// A thing's tag replays at the thing every turn while it is active; when its thing goes it becomes a dead object's
/// tag. A point's tag plays once and goes when its sample stops; a 3D one with a delay first waits for the sound to
/// reach the camera.
class SoundTagSystemInterface
{
public:
	/// audio::tags::TagId; 0 is no tag
	using TagId = uint32_t;

	virtual ~SoundTagSystemInterface() = default;

	/// Once a game turn, from the audio's end of turn: every tag, newest first
	virtual void ProcessTurn() = 0;
	/// An active tag switched off stops its sample
	virtual void SetActive(TagId tag, bool active) = 0;
	/// A point's tag in a sound effect bank: it plays at once at the point, unless it is 3D with a delay
	virtual TagId CreatePointSound(const glm::vec3& point, int sample, int mode, int loops, bool extra3DFlag, bool is3D,
	                               audio::SfxBank bank, int delay) = 0;
	/// The same in an ambient bank (the weather's thunder)
	virtual TagId CreatePointSound(const glm::vec3& point, int sample, int mode, int loops, bool extra3DFlag, bool is3D,
	                               audio::AtmosType bank, int delay) = 0;
	/// A thing's tag; it sounds at the thing's point plus the offset, from the next ProcessTurn
	virtual TagId Create(entt::entity thing, const glm::vec3& offset, int sample, bool track, int mode, int loops,
	                     bool extra3DFlag, bool is3D, audio::SfxBank bank, int delay) = 0;
	/// Where an object that is not a game thing sounds from (the interface's: the newest gesture sample's land point);
	/// nullopt when it answers without giving a point
	using PointSource = std::function<std::optional<glm::vec3>()>;
	/// A tag of an object that is not a game thing and never stops working, found by `key` as a thing's tag is (Remove):
	/// it sounds at the source's point, and at the last point it gave when it gives none. A fake that keeps no points
	/// makes it a tag of `key`.
	virtual TagId CreateForObject(entt::entity key, PointSource /*source*/, int sample, bool track, int mode, int loops,
	                              bool extra3DFlag, bool is3D, audio::SfxBank bank, int delay)
	{
		return Create(key, glm::vec3(0.0f), sample, track, mode, loops, extra3DFlag, is3D, bank, delay);
	}
	/// A script marker's tag at a point: it replays like a thing's tag, and its thing never goes
	virtual TagId CreateAtMarker(const glm::vec3& point, int sample, int mode, int loops, bool is3D, audio::SfxBank bank) = 0;
	/// Every tag of that thing, sample and bank is deleted, its sample stopped first when `stop`
	virtual void Remove(entt::entity thing, int sample, audio::SfxBank bank, bool stop) = 0;
	/// The thing forgotten: a sample that plays with loops has its loop released and the tag lives on until it stops;
	/// otherwise the tag goes at once (its sample plays on)
	virtual void Delete(TagId tag) = 0;
	/// The tag still exists
	[[nodiscard]] virtual bool Exists(TagId tag) const = 0;
	/// Every tag stopped and forgotten (a new map)
	virtual void Clear() = 0;
	/// The tag's sound position for its channel: its thing's; nullopt with no thing, so the channel keeps its point
	[[nodiscard]] virtual std::optional<glm::vec3> TagSoundPoint(TagId tag) const = 0;
	/// The tag's own point; nullopt for an unknown tag
	[[nodiscard]] virtual std::optional<glm::vec3> Point(TagId tag) const = 0;
	/// The newest tag of that thing, sample and sound effect bank, if it has one (read only, for the debug readouts)
	[[nodiscard]] virtual std::optional<TagId> FindThingTag(entt::entity /*thing*/, int /*sample*/,
	                                                        audio::SfxBank /*bank*/) const
	{
		return std::nullopt;
	}
	/// The tag's sample is playing on its channel now (read only, for the debug readouts)
	[[nodiscard]] virtual bool Playing(TagId /*tag*/) const { return false; }
};

} // namespace openblack::ecs::systems
