/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureTurnPose.h"

#include <algorithm>
#include <iterator>
#include <utility>

using namespace openblack;
using openblack::ecs::components::CreatureTurnPose;

namespace
{
/// The bone after this one under the same parent, in the order of their indices; none after the last
std::optional<uint32_t> NextUnderSameParent(std::span<const uint32_t> parents, uint32_t bone)
{
	for (auto other = static_cast<size_t>(bone) + 1; other < parents.size(); ++other)
	{
		if (parents[other] == parents[bone])
		{
			return static_cast<uint32_t>(other);
		}
	}
	return std::nullopt;
}

/// The first bone under this one, the lowest index; none for a bone at an end of the tree
std::optional<uint32_t> FirstUnder(std::span<const uint32_t> parents, uint32_t bone)
{
	for (size_t other = 0; other < parents.size(); ++other)
	{
		if (parents[other] == bone)
		{
			return static_cast<uint32_t>(other);
		}
	}
	return std::nullopt;
}

/// The walk from a bone along the ones that follow it: each end bone's place added to the sum, rounded each time
void AddEnds(std::span<const uint32_t> parents, std::span<const glm::mat4> bones, uint32_t start, glm::vec3& sum,
             uint32_t& count)
{
	for (std::optional<uint32_t> bone = start; bone.has_value(); bone = NextUnderSameParent(parents, *bone))
	{
		if (const auto under = FirstUnder(parents, *bone); under.has_value())
		{
			AddEnds(parents, bones, *under, sum, count);
		}
		else
		{
			const auto& at = bones[*bone][3];
			sum = glm::vec3(at.x + sum.x, at.y + sum.y, at.z + sum.z);
			++count;
		}
	}
}
} // namespace

int32_t creature_turn_pose::Step(int32_t turnMs, float size, bool realSpeed, bool doubled)
{
	int32_t step = turnMs;
	if (!realSpeed)
	{
		float rate = size * 0.5f;
		rate = rate * 0.85f;
		rate = 1.6f - rate;
		step = static_cast<int32_t>(rate * static_cast<float>(turnMs));
	}
	return doubled ? step * 2 : step;
}

bool creature_turn_pose::HasPose(const CreatureTurnPose& pose)
{
	return !pose.current.empty();
}

void creature_turn_pose::Advance(CreatureTurnPose& pose, std::vector<glm::mat4> posed)
{
	const bool first = !HasPose(pose);
	pose.previous = std::move(pose.current);
	pose.current = std::move(posed);
	if (first)
	{
		pose.previous = pose.current;
	}
}

void creature_turn_pose::Freeze(CreatureTurnPose& pose)
{
	pose.frozen = pose.current;
}

void creature_turn_pose::Reconnect(CreatureTurnPose& pose)
{
	pose.frozen.reset();
}

std::span<const glm::mat4> creature_turn_pose::Safe(const CreatureTurnPose& pose)
{
	return pose.frozen.has_value() ? std::span<const glm::mat4>(*pose.frozen) : std::span<const glm::mat4>(pose.current);
}

void creature_turn_pose::BlendInto(std::span<glm::mat4> dst, std::span<const glm::mat4> src, float w, bool first)
{
	const auto bones = std::min(dst.size(), src.size());
	for (size_t bone = 0; bone < bones; ++bone)
	{
		// the twelve floats of the bone's rotation and place; the bottom row stays that of an affine matrix
		for (glm::length_t column = 0; column < 4; ++column)
		{
			for (glm::length_t row = 0; row < 3; ++row)
			{
				const float weighted = w * src[bone][column][row];
				dst[bone][column][row] = first ? weighted : weighted + dst[bone][column][row];
			}
			dst[bone][column][3] = column == 3 ? 1.0f : 0.0f;
		}
	}
}

std::vector<glm::mat4> creature_turn_pose::Drawn(const CreatureTurnPose& pose, float share)
{
	if (!HasPose(pose))
	{
		return {};
	}
	const float f = std::min(share, 2.0f);
	std::vector<glm::mat4> drawn(pose.current.size());
	BlendInto(drawn, pose.current, f, true);
	BlendInto(drawn, pose.previous, 1.0f - f, false);
	return drawn;
}

std::vector<glm::mat4> creature_turn_pose::InWorld(std::span<const glm::mat4> bones, const glm::mat4& world)
{
	std::vector<glm::mat4> placed;
	placed.reserve(bones.size());
	std::ranges::transform(bones, std::back_inserter(placed), [&world](const glm::mat4& bone) { return world * bone; });
	return placed;
}

std::optional<uint32_t> creature_turn_pose::MirrorBone(std::span<const int32_t> fileTail, size_t boneCount, uint32_t bone)
{
	if (bone >= boneCount || fileTail.size() < boneCount)
	{
		return std::nullopt;
	}
	const auto mirror = fileTail[fileTail.size() - boneCount + bone];
	if (mirror < 0 || static_cast<size_t>(mirror) >= boneCount)
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(mirror);
}

std::optional<glm::vec3> creature_turn_pose::LeafMean(std::span<const uint32_t> parents, std::span<const glm::mat4> bones,
                                                      uint32_t start)
{
	if (start >= parents.size() || bones.size() < parents.size())
	{
		return std::nullopt;
	}
	glm::vec3 sum(0.0f);
	uint32_t count = 0;
	AddEnds(parents, bones, start, sum, count);
	const float scale = 1.0f / static_cast<float>(count);
	return glm::vec3(scale * sum.x, scale * sum.y, scale * sum.z);
}
