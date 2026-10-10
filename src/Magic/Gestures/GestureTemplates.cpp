/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GestureTemplates.h"

#include <cstring>

#include <optional>
#include <span>

#include <GestureFile.h>
#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::magic::gestures;

namespace
{
uint32_t TemplateCountOf(const std::vector<uint8_t>& bytes)
{
	uint32_t count;
	std::memcpy(&count, bytes.data(), sizeof(count));
	return count;
}

/// The recogniser's view of a template: every one of the 80 keypoints as stored, the low byte of the three byte-sized
/// fields, and the flags as "not zero"
GestureData TemplateFromFile(const ::openblack::gestures::GestureTemplate& entry)
{
	GestureData data;
	for (size_t s = 0; s < k_MaxSamples; ++s)
	{
		const auto& point = entry.points.at(s);
		data.samples.at(s) = {.x = point.x, .y = point.y, .z = point.z, .turn = point.turn, .direction = point.direction};
	}
	data.count = static_cast<uint8_t>(entry.pointCountField);
	data.gesture = entry.Gesture();
	data.positionMode = entry.PositionModeValue();
	data.checkDirection = entry.ChecksDirection();
	data.allowReverse = entry.AllowsMirror();
	data.checkAspect = entry.ChecksAspectRatio();
	data.aspect = entry.aspectRatio;
	return data;
}
} // namespace

void GestureData::SetToZero()
{
	samples.fill(KeySample {});
	count = 0;
	gesture = k_None;
	positionMode = 0;
	aspect = 0.0f;
	checkDirection = false;
	allowReverse = false;
	checkAspect = false;
}

void GestureData::Append(const KeySample& sample)
{
	if (count < k_MaxSamples)
	{
		samples[count++] = sample;
	}
}

bool openblack::magic::gestures::LoadTemplates(const std::vector<uint8_t>& bytes, std::vector<GestureData>& out)
{
	out.clear();
	if (bytes.size() < sizeof(uint32_t))
	{
		return false;
	}
	const auto count = TemplateCountOf(bytes);
	const size_t needed = sizeof(uint32_t) + (static_cast<size_t>(count) * ::openblack::gestures::GestureFile::k_RecordSize);
	if (bytes.size() < needed)
	{
		return false;
	}
	// bytes after the last record are ignored, as before; the reader wants exactly the records the count announces
	::openblack::gestures::GestureFile file;
	if (file.Open(std::span(bytes).first(needed)) != ::openblack::gestures::GestureFileResult::Success)
	{
		return false;
	}
	out.reserve(file.GetTemplates().size());
	for (const auto& entry : file.GetTemplates())
	{
		out.push_back(TemplateFromFile(entry));
	}
	return true;
}

const std::vector<GestureData>& openblack::magic::gestures::Templates()
{
	// read once from the game's files through the resource cache; without a file system there is no list to read
	if (!Locator::filesystem::has_value())
	{
		static const std::vector<GestureData> k_NoTemplates;
		return k_NoTemplates;
	}
	auto& cache = Locator::resources::value().GetGestureTemplates();
	const auto id = entt::hashed_string("gestures/templates").value();
	if (!cache.Contains(id))
	{
		cache.Load(id, resources::GestureTemplatesLoader::FromDiskTag {});
	}
	return *cache.Handle(id);
}
