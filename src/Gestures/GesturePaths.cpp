/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GesturePaths.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include <GestureFile.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include "Magic/Gestures/GestureBuffer.h"
#include "Magic/Gestures/GestureMatch.h"
#include "Magic/Gestures/GestureTemplateFile.h"

using namespace openblack;
using namespace openblack::gesture;

std::vector<glm::vec2> gesture::Trace(std::span<const glm::vec2> points, float step)
{
	std::vector<glm::vec2> samples;
	for (size_t i = 0; i + 1 < points.size(); ++i)
	{
		const auto from = points[i];
		const auto to = points[i + 1];
		const auto steps = std::max(1, static_cast<int>(glm::length(to - from) / step));
		for (int s = 0; s < steps; ++s)
		{
			// Whole pixels, as a cursor's are
			samples.push_back(glm::round(from + ((to - from) * (static_cast<float>(s) / static_cast<float>(steps)))));
		}
	}
	if (!points.empty())
	{
		samples.push_back(glm::round(points.back()));
	}
	return samples;
}

std::vector<glm::vec2> gesture::TraceTemplate(const gestures::GestureTemplate& gestureTemplate, glm::vec2 middle, float size,
                                              float step)
{
	std::vector<glm::vec2> corners;
	for (const auto& point : gestureTemplate.Points())
	{
		corners.push_back(middle + (glm::vec2(point.x - 0.5f, point.z - 0.5f) * size));
	}
	return Trace(corners, step);
}

std::optional<std::vector<glm::vec2>> gesture::TraceGesture(std::span<const gestures::GestureTemplate> templates,
                                                            GestureType gesture, glm::vec2 middle, float size,
                                                            float screenAspect)
{
	// The recogniser's own list, in the same order, so that every template of the gesture is tried as the game tries them
	std::vector<magic::gestures::GestureData> list;
	list.reserve(templates.size());
	for (const auto& entry : templates)
	{
		list.push_back(magic::gestures::TemplateFromFile(entry));
	}
	const auto wanted = static_cast<magic::gestures::Gesture>(gesture);
	for (const auto& entry : templates)
	{
		if (entry.Gesture() != wanted)
		{
			continue;
		}
		auto path = TraceTemplate(entry, middle, size);
		magic::gestures::GestureSystem samples;
		for (const auto& point : path)
		{
			// The path is in whole pixels already
			samples.AddSample(glm::vec3(0.0f), glm::ivec2(point));
		}
		const auto keys = magic::gestures::BuildFromSystem(samples, screenAspect);
		magic::gestures::Result result;
		if (magic::gestures::MatchGesture(list, wanted, keys, result, screenAspect))
		{
			return path;
		}
	}
	return std::nullopt;
}

std::vector<glm::vec2> gesture::TraceCircle(glm::vec2 middle, float radius, bool clockwise, float step)
{
	// A little over a full turn, as hands draw them
	constexpr int k_Corners = 40;
	constexpr float k_Turns = 1.1f;
	std::vector<glm::vec2> corners;
	for (int i = 0; i <= k_Corners; ++i)
	{
		const auto angle = (static_cast<float>(i) / k_Corners) * k_Turns * 2.0f * std::numbers::pi_v<float>;
		const auto across = std::sin(angle) * radius * (clockwise ? 1.0f : -1.0f);
		corners.push_back(middle + glm::vec2(across, -std::cos(angle) * radius));
	}
	return Trace(corners, step);
}

std::vector<glm::vec2> gesture::TraceScribble(glm::vec2 middle, float width, int strokes, float step)
{
	std::vector<glm::vec2> corners;
	for (int i = 0; i <= strokes; ++i)
	{
		const auto side = (i % 2 == 0) ? -0.5f : 0.5f;
		corners.push_back(middle + glm::vec2(side * width, static_cast<float>(i) * 2.0f));
	}
	return Trace(corners, step);
}

gestures::GestureTemplate gesture::MakeTemplate(GestureType gesture, std::span<const magic::gestures::KeySample> keys,
                                                bool checkDirection, bool allowMirror, bool checkAspect, float screenAspect)
{
	gestures::GestureTemplate made;
	if (keys.empty())
	{
		return made;
	}
	magic::gestures::BoundingBox box {
	    .minX = keys.front().x, .minZ = keys.front().z, .maxX = keys.front().x, .maxZ = keys.front().z};
	for (const auto& key : keys)
	{
		box.minX = std::min(box.minX, key.x);
		box.minZ = std::min(box.minZ, key.z);
		box.maxX = std::max(box.maxX, key.x);
		box.maxZ = std::max(box.maxZ, key.z);
	}
	const auto extent = std::max({box.maxX - box.minX, box.maxZ - box.minZ, 1.0f});
	const auto count = std::min(keys.size(), gestures::GestureTemplate::k_MaxPoints);
	for (size_t i = 0; i < count; ++i)
	{
		made.points.at(i) = {.x = (keys[i].x - box.minX) / extent,
		                     .y = 0.0f,
		                     .z = (keys[i].z - box.minZ) / extent,
		                     .turn = keys[i].turn,
		                     .direction = keys[i].direction};
	}
	// The ends turn nowhere
	made.points.at(0).turn = 0.0f;
	made.points.at(count - 1).turn = 0.0f;
	made.pointCountField = static_cast<uint32_t>(count);
	made.gestureField = static_cast<uint32_t>(gesture);
	made.positionModeField = 2;
	made.checkDirection = checkDirection ? 1 : 0;
	made.allowMirror = allowMirror ? 1 : 0;
	made.checkAspectRatio = checkAspect ? 1 : 0;
	made.aspectRatio = magic::gestures::Aspect(box, screenAspect);
	return made;
}
