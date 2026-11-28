/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <chrono>
#include <optional>
#include <string>

/// (openblack) A debug readout for long runs (OPENBLACK_RESOURCE_TRACE=1, off by default): one "game" log line every so
/// many seconds of real time with what the process holds (memory, handles, GDI and USER objects), the registry's
/// entities, the particle effects and their atoms, and bgfx's counts of its own resources. It only reads, so a run with
/// it on plays the same as one without
namespace openblack::debug::resource_trace
{

struct Sample
{
	uint32_t turn {0};
	uint64_t workingSet {0};   ///< bytes of RAM in use now
	uint64_t privateBytes {0}; ///< bytes committed of its own
	uint32_t handles {0};      ///< the process's open kernel handles
	uint32_t gdiObjects {0};
	uint32_t userObjects {0};
	uint64_t entitiesAlive {0}; ///< the registry's live entities
	uint64_t entitiesSlots {0}; ///< the entity storage's size, live and released
	uint32_t entitiesMade {0};  ///< made since the registry was last emptied
	uint64_t effects {0};       ///< the running particle effects
	uint64_t atoms {0};         ///< their atoms
	uint64_t containers {0};    ///< the spot visual containers
	uint32_t nextEffectId {0};  ///< the id the next effect gets: effects made so far plus one
	uint64_t lightSheets {0};   ///< the sheets of light held, standing and owned
	uint32_t strikeAtoms {0};
	uint32_t arcAtoms {0};
	// bgfx::getStats(): the resources it holds (each has a fixed maximum in bgfx's configuration)
	uint32_t textures {0};
	uint32_t shaders {0};
	uint32_t programs {0};
	uint32_t uniforms {0};
	uint32_t vertexBuffers {0};
	uint32_t indexBuffers {0};
	uint32_t dynamicVertexBuffers {0};
	uint32_t dynamicIndexBuffers {0};
	uint32_t frameBuffers {0};
	uint32_t vertexLayouts {0};
	uint32_t occlusionQueries {0};
	int64_t textureMemory {0};
	int64_t renderTargetMemory {0};
	int64_t gpuMemoryUsed {0}; ///< -INT64_MAX or 0 when the backend does not say
};

/// The interval asked for by OPENBLACK_RESOURCE_TRACE ("1" or any other value: 60 s) and OPENBLACK_RESOURCE_TRACE_SECONDS;
/// none when the first is unset or empty
[[nodiscard]] std::optional<std::chrono::seconds> IntervalFrom(const char* trace, const char* seconds);

/// Reads the sample now: the process part on Windows (zero elsewhere), the registry and particles when their services
/// are there, bgfx when it is initialised (the main thread, after the renderer is up)
[[nodiscard]] Sample Take(uint32_t turn, bool withBgfx);

/// One log line: "Resource trace: turn 812, RAM 900.1 MB private 760.2 MB, handles 512 GDI 40 USER 30, entities ..."
[[nodiscard]] std::string Format(const Sample& sample);

} // namespace openblack::debug::resource_trace
