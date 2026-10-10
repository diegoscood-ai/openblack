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

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/Systems/SoundTagSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class SoundTagSystem final: public SoundTagSystemInterface
{
public:
	/// A sound kept up by a thing or sounded at a point. The tags are not registry components: the system keeps them in
	/// one list, newest first as the original walks them, so a land gets no storage for them and the order of the walk
	/// is the original's.
	struct Tag
	{
		/// The tag's id, which is also the owner of its channel (0 for none)
		TagId id {0};
		/// Its thing; entt::null for none (a point's tag, or a tag whose thing has gone)
		entt::entity thing {entt::null};
		/// A tag of a script marker: a thing that never goes away, at `position`
		bool marker {false};
		/// Where it sounds from (the thing's point when it was made)
		glm::vec3 position {0.0f};
		/// Added to the thing's point by the channel
		glm::vec3 offset {0.0f};
		/// The sample's number in its bank
		int sample {0};
		/// The sound effect bank the sample is in (None for an ambient bank's tag)
		audio::SfxBank bank {};
		/// The ambient bank the sample is in instead, for the ambient point tag (None for every other tag)
		audio::AtmosType ambientBank {};
		/// The channel follows the thing (only with a thing)
		bool track {false};
		int mode {3};
		int loops {0};
		bool extra3DFlag {false};
		bool is3D {false};
		/// A point's sound still on its way to the camera (only when is3D)
		int delay {0};
		bool active {true};
		/// Game turns since it was made
		uint16_t turns {0};
		/// Deleted during a walk of the list, removed after it
		bool gone {false};
	};

	void ProcessTurn() override;
	void SetActive(TagId tag, bool active) override;
	TagId CreatePointSound(const glm::vec3& point, int sample, int mode, int loops, bool extra3DFlag, bool is3D,
	                       audio::SfxBank bank, int delay) override;
	TagId CreatePointSound(const glm::vec3& point, int sample, int mode, int loops, bool extra3DFlag, bool is3D,
	                       audio::AtmosType bank, int delay) override;
	TagId Create(entt::entity thing, const glm::vec3& offset, int sample, bool track, int mode, int loops, bool extra3DFlag,
	             bool is3D, audio::SfxBank bank, int delay) override;
	TagId CreateAtMarker(const glm::vec3& point, int sample, int mode, int loops, bool is3D, audio::SfxBank bank) override;
	void Remove(entt::entity thing, int sample, audio::SfxBank bank, bool stop) override;
	void Delete(TagId tag) override;
	[[nodiscard]] bool Exists(TagId tag) const override;
	void Clear() override;
	[[nodiscard]] std::optional<glm::vec3> TagSoundPoint(TagId tag) const override;
	[[nodiscard]] std::optional<glm::vec3> Point(TagId tag) const override;

private:
	[[nodiscard]] Tag* Find(TagId tag);
	[[nodiscard]] const Tag* Find(TagId tag) const;
	TagId Add(Tag tag);
	/// The point form shared by both kinds of bank
	TagId AddPointSound(Tag tag);
	void Compact();

	/// The tags in creation order (the original's head is the newest, so the turn walks it from the back)
	std::vector<Tag> _tags;
	TagId _nextId {1};
};

} // namespace openblack::ecs::systems
