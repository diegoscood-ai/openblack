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

#include <filesystem>
#include <optional>
#include <string>

namespace openblack
{

/// A testbed scenario asked for on the command line: its id, and for a benchmark the frames to let settle, the frames to
/// measure and where to write the results; a benchmark's game quits once its frames are measured
struct ScenarioRequest
{
	std::string id;
	uint32_t warmUpFrames {120};
	uint32_t frames {600};
	/// --benchmark-out: the results are written only when it is given
	std::optional<std::filesystem::path> results;
};

namespace testbed_scenarios
{

/// The command line's --scenario, --benchmark-warmup, --benchmark-frames and --benchmark-out as a request: none without
/// a scenario, and at least one frame measured
[[nodiscard]] std::optional<ScenarioRequest> MakeScenarioRequest(const std::optional<std::string>& scenario,
                                                                 uint32_t warmUpFrames, uint32_t frames,
                                                                 const std::optional<std::string>& resultsBase);

} // namespace testbed_scenarios
} // namespace openblack
