/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ProcessLogOnce.h"

#include "ECS/Systems/LogOnceInterface.h"
#include "Locator.h"

bool openblack::log_once::First(std::string_view key)
{
	return !Locator::logOnce::has_value() || Locator::logOnce::value().First(key);
}
