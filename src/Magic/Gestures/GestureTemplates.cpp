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

#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"

using namespace openblack::magic::gestures;

namespace
{
constexpr size_t k_RecordSize = 0x65C;
constexpr size_t k_SampleSize = 0x14;

std::optional<std::vector<GestureData>> g_Templates;

template <class T>
T Read(const std::vector<uint8_t>& bytes, size_t offset)
{
	T value;
	std::memcpy(&value, bytes.data() + offset, sizeof(T));
	return value;
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
	if (bytes.size() < 4)
	{
		return false;
	}
	const auto count = Read<uint32_t>(bytes, 0);
	if (bytes.size() < 4 + static_cast<size_t>(count) * k_RecordSize)
	{
		return false;
	}
	out.resize(count);
	for (uint32_t i = 0; i < count; ++i)
	{
		const size_t base = 4 + static_cast<size_t>(i) * k_RecordSize;
		auto& data = out[i];
		for (size_t s = 0; s < k_MaxSamples; ++s)
		{
			const size_t at = base + s * k_SampleSize;
			data.samples[s] = {Read<float>(bytes, at), Read<float>(bytes, at + 4), Read<float>(bytes, at + 8),
			                   Read<float>(bytes, at + 0xC), Read<uint32_t>(bytes, at + 0x10)};
		}
		// fn_005790A0 reads 4 bytes into each of the three byte fields (only the low byte stays)
		data.count = static_cast<uint8_t>(Read<uint32_t>(bytes, base + 0x640));
		data.gesture = static_cast<Gesture>(Read<uint32_t>(bytes, base + 0x644));
		data.positionMode = static_cast<uint8_t>(Read<uint32_t>(bytes, base + 0x648));
		data.checkDirection = Read<uint32_t>(bytes, base + 0x64C) != 0;
		data.allowReverse = Read<uint32_t>(bytes, base + 0x650) != 0;
		data.checkAspect = Read<uint32_t>(bytes, base + 0x654) != 0;
		data.aspect = Read<float>(bytes, base + 0x658);
	}
	return true;
}

const std::vector<GestureData>& openblack::magic::gestures::Templates()
{
	if (!g_Templates)
	{
		g_Templates.emplace();
		if (Locator::filesystem::has_value())
		{
			auto& fileSystem = Locator::filesystem::value();
			// GGame::LoadFiles: ".\Data\Gestures.jty" (0xBEC8A4)
			const auto path = fileSystem.GetPath<filesystem::Path::Data>() / "Gestures.jty";
			try
			{
				if (!LoadTemplates(fileSystem.ReadAll(path), *g_Templates))
				{
					SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Gestures: {} is short", path.generic_string());
				}
			}
			catch (const std::exception& e)
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Gestures: cannot read {}: {}", path.generic_string(), e.what());
			}
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Gestures: {} templates", g_Templates->size());
		}
	}
	return *g_Templates;
}

void openblack::magic::gestures::SetTemplates(std::vector<GestureData> templates)
{
	g_Templates = std::move(templates);
}
