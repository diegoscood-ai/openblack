/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ResourceTrace.h"

#include <charconv>

#include <string_view>

#include <bgfx/bgfx.h>
#include <fmt/format.h>

#include "ECS/Registry.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "Locator.h"
#include "Particles/ParticleManagerState.h"

#if defined(_WIN32)
// (NOMINMAX and WIN32_LEAN_AND_MEAN come from src/CMakeLists.txt)
#include <windows.h>
// after windows.h
#include <psapi.h>
#endif

namespace openblack::debug::resource_trace
{
namespace
{
constexpr double k_Mb = 1024.0 * 1024.0;
constexpr std::chrono::seconds k_DefaultInterval {60};
} // namespace

std::optional<std::chrono::seconds> IntervalFrom(const char* trace, const char* seconds)
{
	if (trace == nullptr || *trace == '\0')
	{
		return std::nullopt;
	}
	if (seconds != nullptr)
	{
		const std::string_view text {seconds};
		int value = 0;
		const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
		if (error == std::errc() && end == text.data() + text.size() && value > 0)
		{
			return std::chrono::seconds {value};
		}
	}
	return k_DefaultInterval;
}

Sample Take(uint32_t turn, bool withBgfx)
{
	Sample sample {.turn = turn};
#if defined(_WIN32)
	const HANDLE process = GetCurrentProcess();
	PROCESS_MEMORY_COUNTERS_EX counters {};
	counters.cb = sizeof(counters);
	if (GetProcessMemoryInfo(process, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)) != 0)
	{
		sample.workingSet = counters.WorkingSetSize;
		sample.privateBytes = counters.PrivateUsage;
	}
	DWORD handles = 0;
	if (GetProcessHandleCount(process, &handles) != 0)
	{
		sample.handles = handles;
	}
	sample.gdiObjects = GetGuiResources(process, GR_GDIOBJECTS);
	sample.userObjects = GetGuiResources(process, GR_USEROBJECTS);
#endif
	if (Locator::entitiesRegistry::has_value())
	{
		const auto& registry = Locator::entitiesRegistry::value();
		bool first = true;
		// the entities' own storage comes first
		registry.EachStorage([&sample, &first](entt::id_type /*id*/, const entt::sparse_set& storage) {
			if (first)
			{
				sample.entitiesAlive = storage.free_list();
				sample.entitiesSlots = storage.size();
				first = false;
			}
		});
		sample.entitiesMade = registry.GetEntitiesMade();
	}
	if (Locator::particleSystem::has_value())
	{
		const auto& state = Locator::particleSystem::value().GetState();
		for (const auto& [id, running] : state.effects)
		{
			++sample.effects;
			if (running.effect != nullptr)
			{
				sample.atoms += running.effect->AtomCount();
			}
		}
		sample.containers = state.containers.size();
		sample.nextEffectId = state.nextId;
		sample.lightSheets = state.lightSheets.size() + state.ownedLightSheets.size();
		sample.strikeAtoms = state.strikeAtoms;
		sample.arcAtoms = state.arcAtoms;
	}
	if (withBgfx)
	{
		if (const auto* stats = bgfx::getStats(); stats != nullptr)
		{
			sample.textures = stats->numTextures;
			sample.shaders = stats->numShaders;
			sample.programs = stats->numPrograms;
			sample.uniforms = stats->numUniforms;
			sample.vertexBuffers = stats->numVertexBuffers;
			sample.indexBuffers = stats->numIndexBuffers;
			sample.dynamicVertexBuffers = stats->numDynamicVertexBuffers;
			sample.dynamicIndexBuffers = stats->numDynamicIndexBuffers;
			sample.frameBuffers = stats->numFrameBuffers;
			sample.vertexLayouts = stats->numVertexLayouts;
			sample.occlusionQueries = stats->numOcclusionQueries;
			sample.textureMemory = stats->textureMemoryUsed;
			sample.renderTargetMemory = stats->rtMemoryUsed;
			sample.gpuMemoryUsed = stats->gpuMemoryUsed;
		}
	}
	return sample;
}

std::string Format(const Sample& sample)
{
	std::string text =
	    fmt::format("Resource trace: turn {}, RAM {:.1f} MB private {:.1f} MB, handles {} GDI {} USER {}", sample.turn,
	                static_cast<double>(sample.workingSet) / k_Mb, static_cast<double>(sample.privateBytes) / k_Mb,
	                sample.handles, sample.gdiObjects, sample.userObjects);
	text += fmt::format(", entities {} of {} (made {})", sample.entitiesAlive, sample.entitiesSlots, sample.entitiesMade);
	text +=
	    fmt::format(", effects {} atoms {} containers {} next id {} sheets {} strikes {} arcs {}", sample.effects, sample.atoms,
	                sample.containers, sample.nextEffectId, sample.lightSheets, sample.strikeAtoms, sample.arcAtoms);
	text += fmt::format(", bgfx textures {} shaders {} programs {} uniforms {} vb {} ib {} dvb {} dib {} fb {} layouts {} "
	                    "queries {}",
	                    sample.textures, sample.shaders, sample.programs, sample.uniforms, sample.vertexBuffers,
	                    sample.indexBuffers, sample.dynamicVertexBuffers, sample.dynamicIndexBuffers, sample.frameBuffers,
	                    sample.vertexLayouts, sample.occlusionQueries);
	text += fmt::format(", VRAM textures {:.1f} MB targets {:.1f} MB", static_cast<double>(sample.textureMemory) / k_Mb,
	                    static_cast<double>(sample.renderTargetMemory) / k_Mb);
	if (sample.gpuMemoryUsed > 0)
	{
		text += fmt::format(" GPU {:.1f} MB", static_cast<double>(sample.gpuMemoryUsed) / k_Mb);
	}
	return text;
}

} // namespace openblack::debug::resource_trace
