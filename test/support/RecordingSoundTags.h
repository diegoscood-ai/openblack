/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// A sound tag service for the tests that records what is asked of it: every thing's tag made, every point tag in a game
// bank and every removal, in order. Everything else does nothing.

#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Audio/Game/BankTables.h"
#include "ECS/Systems/SoundTagSystemInterface.h"

namespace openblack::test
{
/// The sound tags as the hand asks for them: every thing's tag made, every point tag in a game bank and every removal,
/// kept in order
class RecordingSoundTags final: public ecs::systems::SoundTagSystemInterface
{
public:
	struct Made
	{
		int sample;
		bool track;
		int mode;
		int loops;
		bool is3D;
		audio::SfxBank bank;
	};
	struct PointTag
	{
		glm::vec3 point;
		int sample;
		int mode;
		int loops;
		bool extra3DFlag;
		bool is3D;
		audio::SfxBank bank;
		int delay;
	};
	struct Removed
	{
		int sample;
		audio::SfxBank bank;
		bool stop;
	};

	void ProcessTurn() override {}
	void SetActive(TagId /*tag*/, bool /*active*/) override {}
	TagId CreatePointSound(const glm::vec3& point, int sample, int mode, int loops, bool extra3DFlag, bool is3D,
	                       audio::SfxBank bank, int delay) override
	{
		points.push_back({.point = point,
		                  .sample = sample,
		                  .mode = mode,
		                  .loops = loops,
		                  .extra3DFlag = extra3DFlag,
		                  .is3D = is3D,
		                  .bank = bank,
		                  .delay = delay});
		return ++_next;
	}
	TagId CreatePointSound(const glm::vec3& /*point*/, int /*sample*/, int /*mode*/, int /*loops*/, bool /*extra3DFlag*/,
	                       bool /*is3D*/, audio::AtmosType /*bank*/, int /*delay*/) override
	{
		return ++_next;
	}
	TagId Create(entt::entity /*thing*/, const glm::vec3& /*offset*/, int sample, bool track, int mode, int loops,
	             bool /*extra3DFlag*/, bool is3D, audio::SfxBank bank, int /*delay*/) override
	{
		made.push_back({.sample = sample, .track = track, .mode = mode, .loops = loops, .is3D = is3D, .bank = bank});
		return ++_next;
	}
	TagId CreateForObject(entt::entity key, PointSource source, int sample, bool track, int mode, int loops, bool extra3DFlag,
	                      bool is3D, audio::SfxBank bank, int delay) override
	{
		sources.push_back(std::move(source));
		return Create(key, glm::vec3(0.0f), sample, track, mode, loops, extra3DFlag, is3D, bank, delay);
	}
	TagId CreateAtMarker(const glm::vec3& /*point*/, int /*sample*/, int /*mode*/, int /*loops*/, bool /*is3D*/,
	                     audio::SfxBank /*bank*/) override
	{
		return ++_next;
	}
	void Remove(entt::entity /*thing*/, int sample, audio::SfxBank bank, bool stop) override
	{
		removed.push_back({.sample = sample, .bank = bank, .stop = stop});
	}
	void Delete(TagId /*tag*/) override {}
	[[nodiscard]] bool Exists(TagId /*tag*/) const override { return false; }
	void Clear() override {}
	[[nodiscard]] std::optional<glm::vec3> TagSoundPoint(TagId /*tag*/) const override { return std::nullopt; }
	[[nodiscard]] std::optional<glm::vec3> Point(TagId /*tag*/) const override { return std::nullopt; }

	std::vector<Made> made;
	std::vector<Removed> removed;
	/// The point tags in a game bank, in order
	std::vector<PointTag> points;
	/// The point sources of the tags of objects that are not things, in order
	std::vector<PointSource> sources;

private:
	TagId _next {0};
};

} // namespace openblack::test
