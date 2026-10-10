/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedOptions.h"

#include <algorithm>

using namespace openblack;

std::optional<ScenarioRequest> testbed_scenarios::MakeScenarioRequest(const std::optional<std::string>& scenario,
                                                                      uint32_t warmUpFrames, uint32_t frames,
                                                                      const std::optional<std::string>& resultsBase)
{
	if (!scenario.has_value())
	{
		return std::nullopt;
	}
	ScenarioRequest request {
	    .id = *scenario,
	    .warmUpFrames = warmUpFrames,
	    .frames = std::max<uint32_t>(frames, 1),
	};
	if (resultsBase.has_value())
	{
		request.results = std::filesystem::path(*resultsBase);
	}
	return request;
}
