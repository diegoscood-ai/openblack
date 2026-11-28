/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreationDraws.h"

#include <algorithm>
#include <limits>
#include <utility>

using namespace openblack::ecs;

void creation_draws::Run(Queue& queue, entt::entity owner, std::function<void()> draw)
{
	if (!queue.deferring)
	{
		draw();
		return;
	}
	queue.entries.push_back({.owner = owner, .draw = std::move(draw)});
}

void creation_draws::BeginLoad(Queue& queue)
{
	queue.deferring = true;
	queue.entries.clear();
}

creation_draws::Taken creation_draws::EndLoad(Queue& queue, const std::function<std::optional<size_t>(entt::entity)>& rankOf,
                                              const std::function<bool(entt::entity)>& alive)
{
	auto entries = std::exchange(queue.entries, {});
	queue.deferring = false;
	Taken taken {.held = entries.size()};
	std::vector<std::pair<size_t, Queue::Entry*>> ranked;
	ranked.reserve(entries.size());
	for (auto& entry : entries)
	{
		const auto rank = rankOf(entry.owner);
		taken.ranked += rank.has_value() ? 1 : 0;
		ranked.emplace_back(rank.value_or(std::numeric_limits<size_t>::max()), &entry);
	}
	std::ranges::stable_sort(ranked, {}, &std::pair<size_t, Queue::Entry*>::first);
	for (const auto& [rank, entry] : ranked)
	{
		if (alive(entry->owner))
		{
			entry->draw();
			++taken.drawn;
		}
	}
	return taken;
}

void creation_draws::Drop(Queue& queue) noexcept
{
	queue.deferring = false;
	queue.entries.clear();
}
