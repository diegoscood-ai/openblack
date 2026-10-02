/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LanternSounds.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <limits>
#include <map>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "Audio/Audio.h"
#include "Audio/GAudio/AudioSystem.h"
#include "Audio/GameQueries.h"

// A caller of the audio core (the lanterns are ECS things): the lanterns come from GameQueries::streetLanterns, so this
// file reads no ECS component either.

using namespace openblack;
using namespace openblack::audio;

namespace
{
/// LH_SAMPLE_G_LANTERN_01 (InGame.sad 147, 0x734920)
constexpr int k_LanternSample = 0x93;

/// [0xDA0A10]: dark enough for the lanterns to be heard
bool g_On = false;
/// GStreetLantern +0x60, by lantern (the list g_game+0x205C34 of the lanterns)
std::map<entt::entity, tags::TagId> g_Tags;

bool Trace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_LANTERN_SOUND_TRACE") != nullptr;
	return k_Trace;
}
} // namespace

void lantern_sounds::SetOn(bool on)
{
	// fn_007349E0: only when the flag changes does it walk the lanterns (a gone lantern is no longer in their list, so
	// its dead object's tag is not reached)
	if (on == g_On)
	{
		return;
	}
	g_On = on;
	for (const auto& [lantern, tag] : g_Tags)
	{
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Lantern sound: tag {} of lantern {} {}", tag,
			                   static_cast<uint32_t>(lantern), on ? "on" : "off");
		}
		tags::SetActive(tag, on);
	}
}

void lantern_sounds::ProcessTurn()
{
	const auto& query = Queries().streetLanterns;
	if (!query)
	{
		return;
	}
	const auto lanterns = query();
	// GStreetLantern::ToBeDeleted 0x734AB0 only clears +0x60 and unlinks the lantern: its tag sees the thing gone itself
	std::erase_if(g_Tags, [&lanterns](const auto& entry) {
		return !tags::Exists(entry.second) ||
		       std::none_of(lanterns.begin(), lanterns.end(),
		                    [&entry](const StreetLantern& lantern) { return lantern.thing == entry.first; });
	});
	float nearest = std::numeric_limits<float>::max();
	const auto camera = ListenerPoint();
	for (const auto& lantern : lanterns)
	{
		if (camera)
		{
			nearest = std::min(nearest, glm::distance(lantern.position + glm::vec3(0.0f, lantern.height, 0.0f), *camera));
		}
		if (g_Tags.contains(lantern.thing))
		{
			continue;
		}
		// CallVirtualFunctionsForCreation 0x734810 (0x73494E): fn_0071E8C0 with the offset (0, Object::GetHeight 0x638120,
		// 0), then SetActive([0xDA0A10]) 0x734965
		const auto tag = tags::Create(lantern.thing, glm::vec3(0.0f, lantern.height, 0.0f), k_LanternSample, false, 2, -1,
		                              false, true, SfxBank::InGame, 0);
		tags::SetActive(tag, g_On);
		g_Tags.emplace(lantern.thing, tag);
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Lantern sound: tag {} for lantern {} (height {:.2f}, {})", tag,
			                   static_cast<uint32_t>(lantern.thing), lantern.height, g_On ? "on" : "off");
		}
	}
	static uint32_t s_Turn = 0;
	if (Trace() && !lanterns.empty() && s_Turn++ % 50 == 0)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Lantern sound: {} lanterns, dark {}, nearest at {:.1f} (max {:.1f})",
		                   lanterns.size(), g_On, nearest, MaxDistance({Bank(SfxBank::InGame), k_LanternSample}));
	}
}

void lantern_sounds::Clear()
{
	// InitStaticsValues 0x54A84F; the tags themselves go with the map (tags::Clear)
	g_Tags.clear();
	g_On = false;
}
