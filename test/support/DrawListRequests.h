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

#include <vector>

#include "Common/EventManager.h"
#include "ECS/Events/DrawListEvents.h"
#include "Locator.h"
#include "support/RestoreService.h"

namespace openblack::test
{
/// A fresh event manager in the locator for the test's lifetime, which records every draw-list rebuild request in
/// order. The manager the test found is put back when it goes
class DrawListRequests
{
public:
	DrawListRequests()
	{
		Locator::events::emplace<EventManager>();
		Locator::events::value().AddHandler<ecs::events::DrawListRebuildRequested>(
		    [this](const ecs::events::DrawListRebuildRequested& event) { _counts.push_back(event.count); });
	}

	/// The requested counts, oldest first
	[[nodiscard]] const std::vector<uint8_t>& Counts() const { return _counts; }
	void Clear() { _counts.clear(); }

private:
	RestoreService<Locator::events> _restore;
	std::vector<uint8_t> _counts;
};
} // namespace openblack::test
