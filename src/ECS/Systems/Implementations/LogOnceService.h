/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <mutex>
#include <string_view>

#include "Common/LogOnce.h"
#include "ECS/Systems/LogOnceInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// Nothing logged yet when made; the keys are kept under a lock, as the loading threads warn too
class LogOnceService final: public LogOnceInterface
{
public:
	[[nodiscard]] bool First(std::string_view key) override
	{
		const std::lock_guard lock(_mutex);
		return _logged.First(key);
	}

private:
	std::mutex _mutex;
	LogOnce _logged;
};
} // namespace openblack::ecs::systems
