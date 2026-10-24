/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedLandTimeline.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <limits>

#include <glm/geometric.hpp>

using namespace openblack;
using namespace openblack::testbed_land_timeline;

glm::vec2 testbed_land_timeline::Origin(bool onLand, glm::vec2 testbedMiddle)
{
	return onLand ? glm::vec2(0.0f) : testbedMiddle;
}

bool testbed_land_timeline::OnTheLand(glm::vec2 point)
{
	return point.x >= 0.0f && point.y >= 0.0f && point.x <= k_LandSize && point.y <= k_LandSize;
}

std::optional<size_t> testbed_land_timeline::NextChapter(size_t chapter, size_t chapters, bool done)
{
	if (!done || chapter + 1 >= chapters)
	{
		return std::nullopt;
	}
	return chapter + 1;
}

bool testbed_land_timeline::Holds(Compare compare, float value, float target)
{
	switch (compare)
	{
	case Compare::Equal:
		return value == target;
	case Compare::NotEqual:
		return value != target;
	case Compare::Below:
		return value < target;
	case Compare::AtMost:
		return value <= target;
	case Compare::Above:
		return value > target;
	case Compare::AtLeast:
		return value >= target;
	}
	return false;
}

std::string_view testbed_land_timeline::Name(Compare compare)
{
	constexpr std::array<std::string_view, 6> k_Names {"==", "!=", "<", "<=", ">", ">="};
	return k_Names.at(static_cast<size_t>(compare));
}

bool testbed_land_timeline::TaskWaitHolds(TaskWait wait, bool runningNow, bool seenRunning)
{
	switch (wait)
	{
	case TaskWait::Running:
		return runningNow;
	case TaskWait::NotRunning:
		return !runningNow;
	case TaskWait::Finished:
		return !runningNow && seenRunning;
	}
	return false;
}

std::optional<size_t> testbed_land_timeline::GlobalIndex(std::span<const lhvm::VMVar> globals, std::string_view name)
{
	for (size_t i = 1; i < globals.size(); ++i)
	{
		if (globals[i].name == name)
		{
			return i;
		}
	}
	return std::nullopt;
}

float testbed_land_timeline::NumberOf(const lhvm::VMVar& global)
{
	switch (global.type)
	{
	case lhvm::DataType::Int:
		return static_cast<float>(global.value.intVal);
	case lhvm::DataType::Boolean:
		return global.value.intVal != 0 ? 1.0f : 0.0f;
	case lhvm::DataType::Object:
		return static_cast<float>(global.value.uintVal);
	default:
		return global.value.floatVal;
	}
}

lhvm::VMValue testbed_land_timeline::ValueOf(lhvm::DataType type, float number)
{
	switch (type)
	{
	case lhvm::DataType::Int:
		return lhvm::VMValue(static_cast<int32_t>(std::lround(number)));
	case lhvm::DataType::Boolean:
		return lhvm::VMValue(number != 0.0f ? int32_t {1} : int32_t {0});
	case lhvm::DataType::Object:
		return lhvm::VMValue(static_cast<uint32_t>(std::lround(std::max(number, 0.0f))));
	default:
		return lhvm::VMValue(number);
	}
}

bool testbed_land_timeline::ScriptRunning(const std::map<uint32_t, lhvm::VMTask>& tasks, std::string_view script)
{
	return std::ranges::any_of(tasks, [script](const auto& task) { return task.second.name == script; });
}

bool testbed_land_timeline::TextHolds(std::span<const uint32_t> recentTexts, bool currentWaitsForClick, uint32_t text,
                                      bool read)
{
	if (text == 0 || std::ranges::find(recentTexts, text) == recentTexts.end())
	{
		return false;
	}
	return !read || recentTexts.empty() || recentTexts.front() != text || !currentWaitsForClick;
}

bool testbed_land_timeline::CameraWithin(glm::vec3 eye, glm::vec3 point, float radius)
{
	return glm::distance(eye, point) < radius;
}

bool testbed_land_timeline::WithinReach(glm::vec2 at, const ThingNear& pick, glm::vec2 point)
{
	return glm::distance(at, point) <= pick.radius;
}

std::optional<uint32_t> testbed_land_timeline::Nearest(std::span<const ThingFacts> things, const ThingNear& pick,
                                                       glm::vec2 point)
{
	std::optional<uint32_t> found;
	float nearest = std::numeric_limits<float>::max();
	for (const auto& thing : things)
	{
		if (!WithinReach(thing.at, pick, point) ||
		    !ecs::script_type::KindMatches(thing.type, thing.subtype, pick.type, pick.subtype))
		{
			continue;
		}
		if (const float distance = glm::distance(thing.at, point); distance < nearest)
		{
			nearest = distance;
			found = thing.id;
		}
	}
	return found;
}

std::vector<std::string_view> testbed_land_timeline::Names(std::string_view list)
{
	std::vector<std::string_view> names;
	while (!list.empty())
	{
		const auto space = list.find(' ');
		if (const auto name = list.substr(0, space); !name.empty())
		{
			names.push_back(name);
		}
		list = space == std::string_view::npos ? std::string_view {} : list.substr(space + 1);
	}
	return names;
}
