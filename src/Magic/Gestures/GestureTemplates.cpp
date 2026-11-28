/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GestureTemplates.h"

#include <cstddef>
#include <cstring>

#include <bit>
#include <optional>
#include <span>

#include <GestureFile.h>
#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "GestureTemplateFile.h"
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

/// The game's list in memory: one object per template, each its record's size. An object is a table pointer (always the
/// same: the game's code is never moved), a word of 0, the record's 80 samples, then its fields as the reader leaves
/// them: it reads each u32 at the next byte (the count, the gesture), then the position mode, then the three flags at
/// their own words and the aspect last over the position mode's upper half
namespace list_image
{
constexpr size_t k_SampleBytes = sizeof(KeySample);
static_assert(k_SampleBytes == 20);
constexpr size_t k_Record = ::openblack::gestures::GestureFile::k_RecordSize;
constexpr size_t k_SamplesAt = 8;
constexpr size_t k_FieldsAt = k_SamplesAt + (k_MaxSamples * k_SampleBytes);
/// The original's table pointer value at the head of each template object: read as bytes by the overflow past the last
/// keypoint that the gesture matching reproduces, so it is data, not an address openblack uses
constexpr uint32_t k_TablePointer = 0x008DF7E0u;

/// The byte at `offset` of a record's object, from the record's bytes in the file (`record`, k_Record of them)
uint8_t ObjectByte(std::span<const uint8_t> record, size_t offset)
{
	if (offset < 4)
	{
		return static_cast<uint8_t>(k_TablePointer >> (8u * offset));
	}
	if (offset < k_SamplesAt)
	{
		return 0;
	}
	if (offset < k_FieldsAt)
	{
		return record[offset - k_SamplesAt];
	}
	// the file's fields: count, gesture, position mode, check direction, allow reverse, check aspect, aspect
	const size_t fields = k_MaxSamples * k_SampleBytes;
	const auto field = [&](size_t index, size_t byte) { return record[fields + (index * 4) + byte]; };
	switch (const size_t at = offset - k_FieldsAt; at)
	{
	case 0:
		return field(0, 0); // the count's low byte
	case 1:
		return field(1, 0); // the gesture's
	case 2:
	case 3:
		return field(2, at - 2); // the position mode's lower half
	case 4:
	case 5:
	case 6:
	case 7:
		return field(6, at - 4); // the aspect
	default:
		return field(3 + ((at - 8) / 4), (at - 8) % 4); // check direction, allow reverse, check aspect
	}
}

/// The turns of a template's keypoints from the 81st to its count, as far as the list reaches
std::vector<float> LaterTurns(std::span<const uint8_t> records, size_t templateIndex, size_t count)
{
	std::vector<float> turns;
	const size_t listBytes = records.size();
	for (size_t keypoint = k_MaxSamples; keypoint < count; ++keypoint)
	{
		const size_t at = (templateIndex * k_Record) + k_SamplesAt + (keypoint * k_SampleBytes) + offsetof(KeySample, turn);
		if (at + sizeof(float) > listBytes)
		{
			break; // past the list's last object
		}
		uint32_t bits = 0;
		for (size_t byte = 0; byte < sizeof(float); ++byte)
		{
			const size_t where = at + byte;
			const auto object = records.subspan((where / k_Record) * k_Record, k_Record);
			bits |= static_cast<uint32_t>(ObjectByte(object, where % k_Record)) << (8u * byte);
		}
		turns.push_back(std::bit_cast<float>(bits));
	}
	return turns;
}
} // namespace list_image
} // namespace

GestureData openblack::magic::gestures::TemplateFromFile(const ::openblack::gestures::GestureTemplate& entry)
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

std::optional<float> openblack::magic::gestures::TemplateTurn(const GestureData& tpl, int keypoint)
{
	if (keypoint < 0)
	{
		return std::nullopt;
	}
	const auto k = static_cast<size_t>(keypoint);
	if (k < k_MaxSamples)
	{
		return tpl.samples[k].turn;
	}
	if (k - k_MaxSamples < tpl.laterTurns.size())
	{
		return tpl.laterTurns[k - k_MaxSamples];
	}
	return std::nullopt;
}

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
	laterTurns.clear();
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
	if (file.Open(std::span(bytes).first(needed), true) != ::openblack::gestures::GestureFileResult::Success)
	{
		return false;
	}
	const auto records = std::span(bytes).subspan(sizeof(uint32_t), needed - sizeof(uint32_t));
	out.reserve(file.GetTemplates().size());
	for (size_t t = 0; t < file.GetTemplates().size(); ++t)
	{
		out.push_back(TemplateFromFile(file.GetTemplates()[t]));
		if (out.back().count > k_MaxSamples)
		{
			out.back().laterTurns = list_image::LaterTurns(records, t, out.back().count);
		}
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
