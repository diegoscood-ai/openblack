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

#include "Audio.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Rocks.h"
#include "Locator.h"

// A caller of the audio core (the lanterns are ECS things): the core itself reads no ECS component.

using namespace openblack;
using namespace openblack::audio;
using namespace openblack::ecs::components;

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
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// GStreetLantern::ToBeDeleted 0x734AB0 only clears +0x60 and unlinks the lantern: its tag sees the thing gone itself
	std::erase_if(g_Tags, [&registry](const auto& entry) {
		return !registry.Valid(entry.first) || !registry.AllOf<StreetLantern>(entry.first) || !tags::Exists(entry.second);
	});
	size_t count = 0;
	float nearest = std::numeric_limits<float>::max();
	const auto camera = ListenerPoint();
	registry.Each<const StreetLantern, const Transform>(
	    [&](entt::entity entity, const StreetLantern& /*unused*/, const Transform& transform) {
		    ++count;
		    const float height = ecs::Rocks::Height(entity); // Object::GetHeight 0x638120
		    if (camera)
		    {
			    nearest = std::min(nearest, glm::distance(transform.position + glm::vec3(0.0f, height, 0.0f), *camera));
		    }
		    if (g_Tags.contains(entity))
		    {
			    return;
		    }
		    // CallVirtualFunctionsForCreation 0x734810 (0x73494E): fn_0071E8C0, then SetActive([0xDA0A10]) 0x734965
		    const auto tag = tags::Create(entity, glm::vec3(0.0f, height, 0.0f), k_LanternSample, false, 2, -1, false, true,
		                                  SfxBank::InGame, 0);
		    tags::SetActive(tag, g_On);
		    g_Tags.emplace(entity, tag);
		    if (Trace())
		    {
			    SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Lantern sound: tag {} for lantern {} (height {:.2f}, {})", tag,
			                       static_cast<uint32_t>(entity), height, g_On ? "on" : "off");
		    }
	    });
	static uint32_t s_Turn = 0;
	// (openblack test hook, audio session) OPENBLACK_AUDIO_TEST_LANTERN="turn[,distance]": at that turn (counted here) the
	// camera flies to look at the first lantern's top from that distance (3: inside the sample's 5)
	static uint32_t s_HookTurn = 0;
	++s_HookTurn;
	if (const char* hook = std::getenv("OPENBLACK_AUDIO_TEST_LANTERN"); hook != nullptr && Locator::camera::has_value())
	{
		unsigned at = 0;
		float distance = 3.0f;
		if (std::sscanf(hook, "%u,%f", &at, &distance) >= 1 && s_HookTurn == at)
		{
			bool done = false;
			registry.Each<const StreetLantern, const Transform>(
			    [&](entt::entity entity, const StreetLantern& /*unused*/, const Transform& transform) {
				    if (done)
				    {
					    return;
				    }
				    done = true;
				    const glm::vec3 top = transform.position + glm::vec3(0.0f, ecs::Rocks::Height(entity), 0.0f);
				    Locator::camera::value().GetModel().SetFlight(top + glm::vec3(0.0f, distance * 0.35f, distance), top);
				    SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Audio test: camera on lantern {} top ({:.1f}, {:.1f}, {:.1f})",
				                       static_cast<uint32_t>(entity), top.x, top.y, top.z);
			    });
		}
	}
	if (Trace() && count > 0 && s_Turn++ % 50 == 0)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Lantern sound: {} lanterns, dark {}, nearest at {:.1f} (max {:.1f})", count,
		                   g_On, nearest, MaxDistance({Bank(SfxBank::InGame), k_LanternSample}));
	}
}

void lantern_sounds::Clear()
{
	// InitStaticsValues 0x54A84F; the tags themselves go with the map (tags::Clear)
	g_Tags.clear();
	g_On = false;
}
